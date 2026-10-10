#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
有线 HTTP 通信「备用服务器」压力测试脚本
================================================================
被测对象: 设备作为 TCP/HTTP 服务器(默认 192.168.1.30:8088)。
测试场景: 平台的旧 HTTP 连接没有及时断开、设备主槽 client1 仍被占用时,
          平台又发起新连接, 设备应立即强制关闭旧连接并由"备用槽"承接新连接。

本脚本通过"制造滞留/半开的旧连接 + 立即重连"高频命中该分支, 统计成功率、时延与
设备存活情况, 用于验证功能正确性以及长时间稳定性(内存泄漏/看门狗重启/HardFault)。

用法:
  python tools/stress_http.py --ip 192.168.1.30 --port 8088 --duration 3600
  python tools/stress_http.py --mode ws --duration 120
  python tools/stress_http.py --mode halfopen --duration 60 --threads 4

模式(--mode):
  mixed    (默认) 滞留连接重连风暴 + RST 风暴, 多线程混合
  halfopen 仅"滞留旧连接 + 立即重连"(最贴近需求场景)
  rst      仅 RST 异常断开 + 立即重连
  normal   正常请求吞吐(基线对照)
  ws       WebSocket 长连接保持 + ping(验证备用服务器不影响 WebSocket)

输出: 运行日志打印到 stdout; 结束时打印 JSON 汇总。退出码 0=通过, 1=失败, 2=不可连接。
"""

import argparse
import base64
import json
import os
import socket
import struct
import sys
import threading
import time
from collections import deque

# --------------------------------------------------------------------------- #
# 报文构造
# --------------------------------------------------------------------------- #
def build_http_post(ip, port):
    """与固件 com_http_queue_find_info 期望的报文匹配: 含 Host: <ip>:<port> 且以 \\r\\n\\r\\n 结束。"""
    return ("POST /iot/global/0-global/model/service/operate/BoxSubModelMgr/"
            "GetSmartBoxDevList HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: 0\r\n\r\n" % (ip, port)).encode()


def build_ws_request(ip, port):
    """WebSocket 升级请求: 需含 'Connection: Upgrade' 与 'Sec-WebSocket-Key:'。"""
    key = base64.b64encode(os.urandom(16)).decode()
    return ("GET / HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: %s\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n" % (ip, port, key)).encode()


# 客户端 ping 帧(掩码位已置, 掩码键全 0, 载荷 0 字节)
WS_PING = b"\x89\x80\x00\x00\x00\x00"


# --------------------------------------------------------------------------- #
# 基础网络操作
# --------------------------------------------------------------------------- #
def tcp_connect(ip, port, timeout=3.0):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    s.settimeout(timeout)
    s.connect((ip, port))
    return s


def close_quiet(s):
    try:
        s.close()
    except OSError:
        pass


def abort_close(s):
    """SO_LINGER=0 后关闭 => 发送 RST, 模拟平台异常掉线(设备侧不收到正常 FIN)。"""
    try:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
    except OSError:
        pass
    close_quiet(s)


def try_http(args, req):
    """开一条新连接发一条 HTTP 请求并读取响应。返回 (是否成功, 响应字节数)。"""
    s = None
    try:
        s = tcp_connect(args.ip, args.port, args.timeout)
        s.sendall(req)
        data = s.recv(1024)
        return (len(data) > 0), len(data)
    except OSError:
        return False, 0
    finally:
        if s is not None:
            close_quiet(s)


def probe_alive(args):
    """仅探测端口是否可连接(用于发现设备重启窗口)。"""
    s = None
    try:
        s = tcp_connect(args.ip, args.port, args.timeout)
        return True
    except OSError:
        return False
    finally:
        if s is not None:
            close_quiet(s)


# --------------------------------------------------------------------------- #
# 统计
# --------------------------------------------------------------------------- #
class Stats:
    def __init__(self):
        self.lock = threading.Lock()
        self.cycles = 0
        self.ok = 0
        self.fail = 0
        self.conn_fail = 0
        self.bytes = 0
        self.lat = deque(maxlen=5000)
        self.down_events = 0
        self.down_since = None

    def record(self, ok, dt=0.0, nbytes=0, conn_fail=False):
        with self.lock:
            self.cycles += 1
            if ok:
                self.ok += 1
                self.bytes += nbytes
                self.lat.append(dt * 1000.0)
            else:
                self.fail += 1
                if conn_fail:
                    self.conn_fail += 1

    def mark_down(self):
        with self.lock:
            self.down_events += 1

    def snapshot(self):
        with self.lock:
            lat = sorted(self.lat)
            avg = (sum(lat) / len(lat)) if lat else 0.0
            p95 = lat[int(len(lat) * 0.95)] if lat else 0.0
            return dict(cycles=self.cycles, ok=self.ok, fail=self.fail,
                        conn_fail=self.conn_fail, bytes=self.bytes,
                        avg=avg, p95=p95, down_events=self.down_events)


# --------------------------------------------------------------------------- #
# 工作线程
# --------------------------------------------------------------------------- #
def http_worker(idx, args, stats, deadline, stop_evt):
    """mixed / halfopen / rst / normal 模式的工作线程。"""
    req = build_http_post(args.ip, args.port)
    stale = []  # 故意滞留不关闭的旧连接, 用于占用设备主槽

    use_halfopen = (args.mode in ("halfopen", "mixed"))
    use_rst = (args.mode in ("rst", "mixed"))
    # mixed: 偶数线程做 halfopen, 奇数线程做 rst; halfopen/rst: 全部同一种
    if args.mode == "mixed":
        do_halfopen = (idx % 2 == 0)
        do_rst = (idx % 2 == 1)
    else:
        do_halfopen = use_halfopen
        do_rst = use_rst

    while not stop_evt.is_set() and time.time() < deadline:
        try:
            if do_halfopen:
                # 1) 制造滞留旧连接(只发半包, 不关闭) => 占用设备主槽
                try:
                    s_old = tcp_connect(args.ip, args.port, args.timeout)
                    s_old.sendall(req[:16])
                    stale.append(s_old)
                except OSError:
                    pass
                # 2) 立即开新连接请求 => 应由备用服务器承接
                t0 = time.perf_counter()
                ok, n = try_http(args, req)
                stats.record(ok, time.perf_counter() - t0, n)

            if do_rst:
                # RST 异常断开, 制造"设备侧半开连接"
                try:
                    s = tcp_connect(args.ip, args.port, args.timeout)
                    s.sendall(req[:16])
                    abort_close(s)
                except OSError:
                    stats.record(False, 0, 0, conn_fail=True)
                t0 = time.perf_counter()
                ok, n = try_http(args, req)
                stats.record(ok, time.perf_counter() - t0, n)

            if args.mode == "normal":
                t0 = time.perf_counter()
                ok, n = try_http(args, req)
                stats.record(ok, time.perf_counter() - t0, n)

        except Exception:  # noqa: BLE001 - 压测中任何异常都计为失败, 不中断
            stats.record(False, 0, 0, conn_fail=True)

        # 控制 PC 侧残留 socket 数量, 避免本机 fd 耗尽
        while len(stale) > args.max_stale:
            close_quiet(stale.pop(0))

        # 速率控制
        if args.pause > 0:
            time.sleep(args.pause)

    for s in stale:
        close_quiet(s)
    stale.clear()


def ws_worker(args, stats, deadline, stop_evt):
    """ws 模式: 建立 WebSocket 长连接并用 ping 保活, 验证备用服务器改动未影响 WebSocket。"""
    req = build_ws_request(args.ip, args.port)
    while not stop_evt.is_set() and time.time() < deadline:
        s = None
        try:
            s = tcp_connect(args.ip, args.port, args.timeout)
            s.sendall(req)
            resp = s.recv(1024)
            if b"101" in resp:
                stats.record(True, 0, len(resp))
                s.settimeout(5)
                hold_until = time.time() + min(30, max(3, deadline - time.time()))
                while time.time() < hold_until and not stop_evt.is_set():
                    s.sendall(WS_PING)
                    try:
                        pong = s.recv(64)
                        if not pong:          # 对端关闭
                            break
                    except socket.timeout:
                        pass                  # 无 pong 也算保持, 继续
                    time.sleep(1)
            else:
                stats.record(False, 0, len(resp))
        except OSError:
            stats.record(False, 0, 0, conn_fail=True)
        finally:
            if s is not None:
                close_quiet(s)


# --------------------------------------------------------------------------- #
# 主流程
# --------------------------------------------------------------------------- #
def main():
    ap = argparse.ArgumentParser(description="有线 HTTP 备用服务器压力测试")
    ap.add_argument("--ip", default="192.168.1.30")
    ap.add_argument("--port", type=int, default=8088)
    ap.add_argument("--duration", type=int, default=3600, help="压测时长(秒), 默认 3600")
    ap.add_argument("--mode", choices=["mixed", "halfopen", "rst", "normal", "ws"],
                    default="mixed")
    ap.add_argument("--threads", type=int, default=4)
    ap.add_argument("--timeout", type=float, default=3.0, help="单次连接/收发超时(秒)")
    ap.add_argument("--max-stale", type=int, default=32, help="每线程最多滞留的旧连接数")
    ap.add_argument("--pause", type=float, default=0.0, help="每轮之间的间隔(秒), 用于控制压力速率")
    ap.add_argument("--interval", type=int, default=30, help="进度打印间隔(秒)")
    ap.add_argument("--fail-rate-limit", type=float, default=1.0,
                    help="失败率上限(%%), 超过则判定不通过")
    args = ap.parse_args()

    print("=== 有线 HTTP 备用服务器压力测试 ===", flush=True)
    print("target = %s:%d  mode=%s  threads=%d  duration=%ds" %
          (args.ip, args.port, args.mode, args.threads, args.duration), flush=True)

    # --- 预检 ---
    try:
        s = tcp_connect(args.ip, args.port, args.timeout)
        s.sendall(build_http_post(args.ip, args.port))
        resp = s.recv(256)
        close_quiet(s)
        first = resp.split(b"\r\n", 1)[0].decode("latin-1")
        print("[precheck] 可连接; 应答首行: %s" % (first or "<空>"), flush=True)
        if not resp:
            print("[precheck] 警告: 未收到应答, 请确认 Host 与设备 IP:端口 一致", flush=True)
    except OSError as e:
        print("[precheck] 无法连接 %s:%d : %s" % (args.ip, args.port, e), flush=True)
        return 2

    stats = Stats()
    stop_evt = threading.Event()
    deadline = time.time() + args.duration
    t_start = time.time()

    workers = []
    if args.mode == "ws":
        for _ in range(max(1, args.threads)):
            workers.append(threading.Thread(target=ws_worker,
                                            args=(args, stats, deadline, stop_evt),
                                            daemon=True))
    else:
        for i in range(max(1, args.threads)):
            workers.append(threading.Thread(target=http_worker,
                                            args=(i, args, stats, deadline, stop_evt),
                                            daemon=True))
    for w in workers:
        w.start()

    # --- 监控循环: 周期打印进度 + 存活探测 ---
    down_now = False
    try:
        while time.time() < deadline:
            sleep_s = min(args.interval, max(0.5, deadline - time.time()))
            time.sleep(sleep_s)

            alive = probe_alive(args)
            if not alive and not down_now:
                down_now = True
                stats.mark_down()
                print("[!] 设备端口无响应 (可能重启/HardFault)", flush=True)
            elif alive and down_now:
                down_now = False
                print("[+] 设备端口恢复响应", flush=True)

            snap = stats.snapshot()
            print("[%5ds] cycles=%d ok=%d fail=%d connfail=%d down=%d "
                  "avg=%.1fms p95=%.1fms" %
                  (int(time.time() - t_start), snap["cycles"], snap["ok"], snap["fail"],
                   snap["conn_fail"], snap["down_events"], snap["avg"], snap["p95"]),
                  flush=True)
    except KeyboardInterrupt:
        print("[!] 收到中断, 提前结束", flush=True)

    stop_evt.set()
    for w in workers:
        w.join(timeout=5)

    # --- 汇总 ---
    snap = stats.snapshot()
    elapsed = time.time() - t_start
    total = snap["ok"] + snap["fail"]
    fail_rate = (snap["fail"] * 100.0 / total) if total else 100.0
    result = {
        "target": "%s:%d" % (args.ip, args.port),
        "mode": args.mode,
        "elapsed_sec": round(elapsed, 1),
        "cycles": snap["cycles"],
        "ok": snap["ok"],
        "fail": snap["fail"],
        "conn_fail": snap["conn_fail"],
        "fail_rate_pct": round(fail_rate, 3),
        "avg_ms": round(snap["avg"], 2),
        "p95_ms": round(snap["p95"], 2),
        "resp_bytes": snap["bytes"],
        "device_down_events": snap["down_events"],
        "pass": (fail_rate <= args.fail_rate_limit and snap["down_events"] == 0),
    }
    print("=== SUMMARY ===", flush=True)
    print(json.dumps(result, ensure_ascii=False, indent=2), flush=True)
    return 0 if result["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
