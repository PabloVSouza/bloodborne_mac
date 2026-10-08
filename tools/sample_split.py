#!/usr/bin/env python3
"""tools/sample_split.py SAMPLE.txt [threads] [--functions]: per-thread self time from a macOS
`sample` call graph, split into translated guest code (the JIT's unnamed code), bbcpu (interpreter,
fallbacks, dispatcher), the GPU library, other host code and waiting (kernel waits). Busiest threads
first; --functions lists each one's top functions."""
import re
import sys
from collections import defaultdict

WAITS = ('__psynch_cvwait', '__semwait_signal', '__workq_kernreturn', 'mach_msg2_trap', '__ulock_wait',
         'semaphore_wait_signal_trap', '__psynch_mutexwait', 'swtch_pri', 'kevent', '__select', 'poll',
         'semaphore_timedwait_trap', '__nanosleep', '__psynch_rw_rdlock', '__psynch_rw_wrlock')
BBCPU = ('jit_fallback', 'bbcpu_', 'vread', 'vwrite', 'fcompare', 'profile_note', 'interp', 'step',
         'x86_to_jit', 'jit_to_x86', 'translate', 'link_exit', 'call_host', 'exec_', 'flags_', 'alu',
         'unary', 'shift', 'multiply', 'divide', 'bit_', 'cmpxchg', 'bmi', 'string_op', 'x87')

FUNCTIONS = '--functions' in sys.argv

def classify(frame):
    name = frame.split('  (in ')[0].strip()
    if any(name.startswith(w) for w in WAITS):
        return 'waiting'
    if FUNCTIONS:
        m = re.search(r'\(in ([^)]+)\)', frame)
        binary = m.group(1) if m else '?'
        if name.startswith('???') and binary == '<unknown binary>':
            return 'jit code'
        return binary + ': ' + re.split(r' \+ \d', name)[0]
    m = re.search(r'\(in ([^)]+)\)', frame)
    binary = m.group(1) if m else '?'
    if name.startswith('???') and binary == '<unknown binary>':
        return 'jit code'
    if any(name.startswith(w) for w in WAITS):
        return 'waiting'
    if binary == 'bb-probe':
        return 'bbcpu/runtime:' + name
    if binary.startswith('libbbgpu'):
        return 'gpu library'
    return 'host:' + binary

def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    path = args[0]
    want = int(args[1]) if len(args) > 1 else 8
    lines = open(path, errors='replace').read().split('\n')
    start = next(i for i, l in enumerate(lines) if l.startswith('Call graph:'))
    threads = []
    stack = []  # (depth, count, child_sum, key)
    cur = None
    def close_to(depth):
        while stack and stack[-1][0] >= depth:
            d, count, child, key = stack.pop()
            if cur is not None and count - child > 0:
                cur['self'][key] += count - child
            if stack:
                stack[-1][2] += count
    for l in lines[start + 1:]:
        if not l.strip():
            close_to(0)
            if l == '' and cur is not None and not stack:
                pass
            continue
        if l.startswith('Total number in stack') or l.startswith('Sort by top of stack'):
            break
        m = re.match(r'^(\s*)([+!:| ]*)(\d+)\s+(.*)$', l)
        if not m:
            continue
        depth = len(m.group(1)) + len(m.group(2))
        count = int(m.group(3))
        frame = m.group(4)
        if re.match(r'Thread_\d+', frame):
            close_to(0)
            cur = {'name': frame, 'total': count, 'self': defaultdict(int)}
            threads.append(cur)
            continue
        close_to(depth)
        stack.append([depth, count, 0, classify(frame)])
    close_to(0)
    threads.sort(key=lambda t: -(t['total'] - t['self'].get('waiting', 0)))
    if FUNCTIONS:
        for t in threads[:want]:
            print(t['name'][:70])
            for k, v in sorted(t['self'].items(), key=lambda x: -x[1])[:16]:
                print(f'   {v:6d} {k[:110]}')
        return
    for t in threads[:want]:
        busy = t['total'] - t['self'].get('waiting', 0)
        groups = defaultdict(int)
        for k, v in t['self'].items():
            groups[k.split(':')[0]] += v
        top_bbcpu = sorted(((v, k.split(':', 1)[1]) for k, v in t['self'].items() if k.startswith('bbcpu/runtime:')), reverse=True)[:6]
        print(f"{t['name'][:70]}: {t['total']} samples, busy {busy}")
        print('   ' + ', '.join(f'{k} {v}' for k, v in sorted(groups.items(), key=lambda x: -x[1])))
        if top_bbcpu:
            print('   bb-probe top: ' + ', '.join(f'{n} {v}' for v, n in top_bbcpu))

main()
