"""Read-only hardware check: fresh PT100 samples while HTTP waits on a slow body.

Run: python scripts/check_runtime.py http://192.168.50.133
Requires IDLE and outputs OFF. Never sends an actuator or settings command.
"""
import argparse
import json
import socket
import time
import urllib.parse
import urllib.request

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('url')
parser.add_argument('--seconds', type=int, default=10)
args = parser.parse_args()
if not 5 <= args.seconds <= 30:
    parser.error('--seconds must be between 5 and 30')
base = args.url.rstrip('/')
target = urllib.parse.urlparse(base)
if target.scheme != 'http' or not target.hostname:
    parser.error('Expected http://board-address')

def status():
    with urllib.request.urlopen(base + '/status', timeout=8) as response:
        return json.load(response)

before = status()
assert before['processStatus'] == 'Idle' and not before['gasOn'] and not before['pumpOn'], 'Stop process and outputs before testing'
assert before['control']['watchdog'], 'Control watchdog not registered'
for channel in ('pot', 'valve'):
    assert before['sensors'][channel]['valid'], f'{channel}: no valid baseline'

# Unknown route: consumes HTTP input but never invokes a product mutation.
with socket.create_connection((target.hostname, target.port or 80), timeout=5) as client:
    client.sendall((f'POST /runtime-read-only-probe HTTP/1.1\r\nHost: {target.hostname}\r\n'
                    'Content-Type: application/x-www-form-urlencoded\r\n'
                    'Content-Length: 100\r\nConnection: close\r\n\r\nx=').encode())
    started = time.monotonic()
    for _ in range(args.seconds):
        client.sendall(b'x')
        time.sleep(1)
    elapsed = time.monotonic() - started
time.sleep(1)
after = status()
assert after['control']['cycles'] - before['control']['cycles'] >= elapsed * 150, 'Control stalled or restarted'
assert after['control']['maxGapMs'] <= 100, 'Observed control gap exceeds 100 ms target (includes boot/maintenance)'
summary = {'elapsedSeconds': round(elapsed, 2), 'control': after['control'], 'sensors': {}}
for channel in ('pot', 'valve'):
    old, new = before['sensors'][channel], after['sensors'][channel]
    assert new['valid'] and new['ageMs'] <= 1500, f'{channel}: stale or invalid'
    assert new['samples'] - old['samples'] >= args.seconds - 1, f'{channel}: missed sample cycles'
    assert new['totalFailures'] == old['totalFailures'], f'{channel}: new measurement fault'
    summary['sensors'][channel] = {'value': new['value'], 'ageMs': new['ageMs'], 'newSamples': new['samples'] - old['samples'], 'totalFailures': new['totalFailures']}
assert not after['gasOn'] and not after['pumpOn'], 'Unexpected output state'
print(json.dumps(summary, indent=2))
print('PASS: sampling and control continued during slow HTTP input; outputs remained OFF')
