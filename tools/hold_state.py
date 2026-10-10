#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""持续制造"旧连接占位 + 新连接请求", 便于在运行中通过 SWD 抓取设备内存状态。"""
import socket
import sys
import time

IP = sys.argv[1] if len(sys.argv) > 1 else "192.168.1.30"
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 8088
ROUNDS = int(sys.argv[3]) if len(sys.argv) > 3 else 90

REQ = ("POST /iot/global/0-global/model/service/operate/BoxSubModelMgr/"
       "GetSmartBoxDevList HTTP/1.1\r\n"
       "Host: %s:%d\r\nContent-Type: application/json\r\nContent-Length: 0\r\n\r\n"
       % (IP, PORT)).encode()


def conn():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    s.settimeout(4.0)
    s.connect((IP, PORT))
    return s


old = []
for i in range(ROUNDS):
    # 旧连接: 只发半包, 保持打开 => 占用主槽
    try:
        o = conn()
        o.sendall(REQ[:16])
        old.append(o)
    except OSError as e:
        print(i, "old err", e, flush=True)
    # 新连接: 立刻请求
    try:
        n = conn()
        n.sendall(REQ)
        try:
            d = n.recv(128)
            print(i, "new recv", len(d), d.split(b"\r\n", 1)[0].decode("latin-1"), flush=True)
        except socket.timeout:
            print(i, "new recv TIMEOUT", flush=True)
        except OSError as e:
            print(i, "new recv err", e, flush=True)
        n.close()
    except OSError as e:
        print(i, "new conn err", e, flush=True)
    time.sleep(1.0)

print("DONE", flush=True)
time.sleep(5)
