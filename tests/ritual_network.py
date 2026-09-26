"""Real-time smoke test: python tests/ritual_network.py path/to/zioop_ritual_server.exe.

Runs for about 60 seconds on an automatically selected local port.
"""

import json
import select
import socket
import struct
import subprocess
import sys
import tempfile
import time
from pathlib import Path

HELLO, MOVE, LOOK, TAKE, INVENTORY, RITUAL, DISCONNECT, GET_MATRIX = 0, 1, 3, 4, 8, 9, 10, 11
OK, KO = 0, 1


def send(client, command, payload):
    data = json.dumps(payload).encode()
    client.sendall(struct.pack("!HI", command, len(data)) + data)


def read_exact(client, size):
    data = bytearray()
    while len(data) < size:
        chunk = client.recv(size - len(data))
        assert chunk, "Unexpected disconnection"
        data.extend(chunk)
    return data


def receive(client):
    kind, size = struct.unpack("!HI", read_exact(client, 6))
    return kind, read_exact(client, size).decode()


def request(client, command, payload, expected=OK):
    send(client, command, payload)
    kind, body = receive(client)
    assert kind == expected, (command, kind, body)
    return json.loads(body) if kind == OK else body


def collect(client, agent_id):
    x, y = 11, 11
    count = 0
    while y >= 0:
        cells = request(client, LOOK, {"agentID": agent_id, "direction": 0})["cells"]
        for _ in cells[0]["resources"]:
            request(client, TAKE, {"agentID": agent_id, "type": 0})
            count += 1
            if count == 10:
                return x, y
        if y % 2 == 1 and x > 0:
            direction, x = 270, x - 1
        elif y % 2 == 0 and x < 11:
            direction, x = 180, x + 1
        else:
            direction, y = 0, y - 1
        assert y >= 0, "Not enough fruit on test map"
        request(client, MOVE, {"agentID": agent_id, "direction": direction})
    raise AssertionError("Collection did not finish")


def main():
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]

    clients = []
    with tempfile.TemporaryFile(mode="w+b") as log:
        server = subprocess.Popen(
            [str(Path(sys.argv[1]).resolve()), str(port)],
            stdin=subprocess.PIPE, stdout=log, stderr=log,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
        try:
            def connect():
                client = socket.create_connection(("127.0.0.1", port), timeout=3)
                clients.append(client)
                return client

            deadline = time.monotonic() + 5
            while True:
                assert server.poll() is None, "Test server failed to start"
                try:
                    bot = connect()
                    break
                except ConnectionRefusedError:
                    if time.monotonic() >= deadline:
                        raise
                    time.sleep(0.05)

            agent_id = request(bot, HELLO, {})["agentID"]
            request(bot, RITUAL, {"agentID": agent_id}, KO)
            request(bot, RITUAL, {}, KO)
            request(bot, RITUAL, {"agentID": "invalid"}, KO)
            x, _ = collect(bot, agent_id)
            start = time.monotonic()
            send(bot, RITUAL, {"agentID": agent_id})
            assert not select.select([bot], [], [], 0.2)[0], "Premature success response"
            request(bot, MOVE, {"agentID": agent_id, "direction": 0}, KO)
            request(bot, TAKE, {"agentID": agent_id, "type": 0}, KO)
            assert not select.select([bot], [], [], 0.1)[0], "Duplicate TAKE response"
            request(bot, RITUAL, {"agentID": agent_id}, KO)
            request(bot, LOOK, {"agentID": agent_id, "direction": 0})
            assert request(bot, INVENTORY, {"agentID": agent_id})["resources"] == []

            other = connect()
            other_id = request(other, HELLO, {})["agentID"]
            request(other, RITUAL, {"agentID": agent_id}, KO)
            collect(other, other_id)  # Other agents can move and collect during the ritual.
            send(other, RITUAL, {"agentID": other_id})
            assert not select.select([other], [], [], 0.1)[0]
            send(other, DISCONNECT, {})
            other.close()

            observer = connect()
            removal_deadline = time.monotonic() + 3
            while True:
                agents = request(observer, GET_MATRIX, {})["matrix"]["data"]["agents"]
                if all(agent["agentId"] != other_id for agent in agents):
                    break
                assert time.monotonic() < removal_deadline, "Disconnected ritual agent remains"
                time.sleep(0.05)
            print("Network refusals, concurrent agents and disconnection passed; waiting for level 2...", flush=True)

            deadline = start + 65
            while not select.select([bot], [], [], 0.5)[0]:
                assert time.monotonic() < deadline, "Ritual completion timed out"
                request(observer, GET_MATRIX, {})  # Verify server responsiveness while waiting.
            kind, body = receive(bot)
            elapsed = time.monotonic() - start
            assert kind == OK and json.loads(body) == {"agentID": agent_id, "level": 2}
            assert elapsed >= 60, f"Ritual completed too early: {elapsed}"
            assert not select.select([bot], [], [], 0.2)[0], "Duplicate completion response"
            request(bot, MOVE, {"agentID": agent_id, "direction": 270 if x > 0 else 180})
            print(f"Network ritual test passed: level 2 after {elapsed:.2f} s", flush=True)

            # A pending ritual must not delay a graceful server shutdown.
            final_bot = connect()
            final_id = request(final_bot, HELLO, {})["agentID"]
            collect(final_bot, final_id)
            send(final_bot, RITUAL, {"agentID": final_id})
            assert not select.select([final_bot], [], [], 0.1)[0]
            shutdown_start = time.monotonic()
            server.stdin.close()
            assert server.wait(timeout=3) == 0, "Graceful server shutdown failed"
            print(f"Shutdown during ritual passed: {time.monotonic() - shutdown_start:.2f} s", flush=True)
        except Exception:
            log.seek(0)
            print(log.read().decode(errors="replace")[-6000:], file=sys.stderr)
            raise
        finally:
            for client in clients:
                client.close()
            if not server.stdin.closed:
                server.stdin.close()
            if server.poll() is None:
                try:
                    server.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    server.terminate()
                    server.wait(timeout=5)


if __name__ == "__main__":
    main()
