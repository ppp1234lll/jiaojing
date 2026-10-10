#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""定向探测: 在主槽被占用时, 新连接能否被备用服务器正常服务。"""
import os
import socket
import sys
import time

IP = sys.argv[1] if len(sys.argv) > 1 else "192.168.1.30"
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 8088

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


def recv_head(s, tag):
    try:
        data = s.recv(256)
        if data:
            print("  [%s] recv %d bytes: %s" % (tag, len(data),
                  data.split(b"\r\n", 1)[0].decode("latin-1")))
            return True
        print("  [%s] recv EMPTY (对端已关闭)" % tag)
        return False
    except socket.timeout:
        print("  [%s] recv TIMEOUT (无应答)" % tag)
        return False
    except OSError as e:
        print("  [%s] recv ERROR: %s" % (tag, e))
        return False


def close_q(s):
    try:
        s.close()
    except OSError:
        pass


def case(name, setup):
    print("\n=== %s ===" % name)
    hold = []
    try:
        setup(hold)
    except OSError as e:
        print("  setup error: %s" % e)
    # 新连接请求
    try:
        s = conn()
        s.sendall(REQ)
        recv_head(s, "new")
        close_q(s)
    except OSError as e:
        print("  new conn error: %s" % e)
    for h in hold:
        close_q(h)
    time.sleep(0.3)


def s_none(hold):
    pass


def s_idle(hold):
    hold.append(conn())          # 只连接, 不发数据


def s_partial(hold):
    s = conn(); s.sendall(REQ[:16]); hold.append(s)


def s_full(hold):
    s = conn(); s.sendall(REQ)
    recv_head(s, "old")          # 让旧连接完整走一遍
    hold.append(s)


def s_two_idle(hold):
    hold.append(conn())
    hold.append(conn())


case("A 无占用(基线)", s_none)
case("B 旧连接仅连接不发数据", s_idle)
case("C 旧连接发半包(16B)", s_partial)
case("D 旧连接发完整请求", s_full)
case("E 连开两条不发数据", s_two_idle)
print("\nDONE")
