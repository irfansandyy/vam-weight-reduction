#!/usr/bin/env python3
"""Benchmark dataset 69. Standard library, GCC, G++, and Make are sufficient."""

from __future__ import annotations

import argparse
import csv
from decimal import Decimal
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import statistics
import subprocess
import tempfile
import time


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
SEED = 2026110407
TIMEOUT = 20
VRB_FLAGS = '-O2 -std=c11 -Wall -Wextra -Wpedantic'
ROSTER = HERE / 'roster.json'
REFERENCES = HERE / 'reference69.jsonl'
FIELDS = (
    'case_id', 'path', 'group', 'family', 'size_bin', 'split', 'model_hash',
    'rows', 'columns', 'method', 'valid', 'status', 'returncode', 'cost',
    'optimum', 'gap_pct', 'optimal', 'allocation_hash', 'rounds_ns', 'median_ns',
    'cost_vrb', 'runtime_vrb',
)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def json_sha(value, *, sort_keys=False):
    return hashlib.sha256(json.dumps(value, separators=(',', ':'), sort_keys=sort_keys).encode()).hexdigest()


def require(ok, message):
    if not ok:
        raise ValueError(message)


def build_methods(build):
    build.mkdir(parents=True, exist_ok=True)
    methods = json.loads(ROSTER.read_text())
    subprocess.run(['make', '-B', '-C', str(ROOT), 'CC=gcc', f'CFLAGS={VRB_FLAGS}', 'build/vrb'], check=True)
    for spec in methods:
        name = spec['method']
        if name == 'VRB':
            binary = ROOT / 'build/vrb'
        else:
            source = HERE / spec['compiled_source']
            require(sha(HERE / spec['source']) == spec['original_sha256'], f'Berkas asli berubah: {name}')
            require(sha(source) == spec['compiled_source_sha256'], f'Adapter berubah: {name}')
            binary = build / name
            subprocess.run([spec['compiler'], *spec['flags'], str(source), '-lm', '-o', str(binary)], check=True)
        spec['binary'] = binary
        spec['binary_sha256'] = sha(binary)
    return methods


def read_case(ref):
    values = list(map(int, (ROOT / ref['path']).read_text().split()))
    rows, cols = values[:2]
    end = 2 + rows * cols
    require(rows > 0 and cols > 0 and len(values) >= end + rows + cols,
            f"Dimensi atau isi kasus salah: {ref['case_id']}")
    costs = [values[2 + i * cols:2 + (i + 1) * cols] for i in range(rows)]
    supply = values[end:end + rows]
    demand = values[end + rows:end + rows + cols]
    require(min(values[2:end + rows + cols]) >= 0, 'Data kasus harus nonnegatif')
    delta = sum(supply) - sum(demand)
    if delta > 0:
        demand.append(delta)
        for row in costs:
            row.append(0)
    if delta < 0:
        supply.append(-delta)
        costs.append([0] * cols)
    rows, cols = len(supply), len(demand)
    name = ref['case_id']
    require(json_sha(dict(costs=costs, supply=supply, demand=demand), sort_keys=True) == ref['model_hash'],
            f'Data kasus berbeda dari referensi: {name}')

    # Recheck the primal and dual LP certificate using exact integers.
    u, v = ref['dual_row'], ref['dual_column']
    optimum = ref['optimum']
    require(len(u) == rows and len(v) == cols and all(type(x) is int for x in u + v + [optimum]),
            f'Sertifikat dual tidak valid: {name}')
    require(all(u[i] + v[j] <= costs[i][j] for i in range(rows) for j in range(cols)),
            f'Dual tidak layak: {name}')
    require(sum(a * b for a, b in zip(u, supply)) + sum(a * b for a, b in zip(v, demand)) == optimum,
            f'Nilai dual tidak sesuai: {name}')
    optimal_allocation = [[0] * cols for _ in range(rows)]
    seen = set()
    for i, j, q in ref['allocation_sparse']:
        require(all(type(x) is int for x in (i, j, q)) and 0 <= i < rows and 0 <= j < cols and q >= 0 and (i, j) not in seen,
                f'Sertifikat primal tidak valid: {name}')
        optimal_allocation[i][j] = q
        seen.add((i, j))
    require(not allocation_errors(optimal_allocation, supply, demand)
            and total_cost(optimal_allocation, costs) == optimum, f'Nilai primal tidak sesuai: {name}')

    keep_rows = [i for i in range(rows) if supply[i] > 0]
    keep_cols = [j for j in range(cols) if demand[j] > 0]
    require(keep_rows and keep_cols, f'Kasus aktif kosong: {name}')
    return dict(costs=[[costs[i][j] for j in keep_cols] for i in keep_rows],
                supply=[supply[i] for i in keep_rows], demand=[demand[j] for j in keep_cols])


def write_case(path, case):
    parts = [[len(case['supply']), len(case['demand'])], *case['costs'], case['supply'], case['demand']]
    path.write_text('\n'.join(' '.join(map(str, row)) for row in parts) + '\n')


def allocation_errors(x, supply, demand):
    rows, cols = len(supply), len(demand)
    if len(x) != rows or any(len(row) != cols for row in x):
        raise ValueError('allocation_shape_mismatch')
    errors = []
    if any(q < 0 for row in x for q in row):
        errors.append('negative_allocation')
    if list(map(sum, x)) != supply:
        errors.append('supply_mismatch')
    if [sum(row[j] for row in x) for j in range(cols)] != demand:
        errors.append('demand_mismatch')
    return errors


def total_cost(x, costs):
    return sum(q * c for allocation, prices in zip(x, costs) for q, c in zip(allocation, prices))


def allocation_integer(value):
    q = Decimal(str(value))
    require(q.is_finite() and q == int(q), 'noninteger_allocation')
    return int(q)


def validate_output(spec, completed, case):
    result = dict(valid=False, cost=None, returncode=completed.returncode, allocation_hash=None)
    if completed.returncode:
        return result | dict(status='process_failure')
    try:
        if spec['format'] == 'json':
            obj = json.loads(completed.stdout, parse_float=Decimal)
            x = obj.get('allocation')
            if x is None:
                return result | dict(status=str(obj.get('error', 'missing_allocation')))
        else:
            if '__RESULT__ ' not in completed.stdout:
                return result | dict(status='missing_allocation_marker')
            lines = completed.stdout.rsplit('__RESULT__ ', 1)[1].splitlines()
            m, n = map(int, lines[0].split())
            require((m, n) == (len(case['supply']), len(case['demand'])), 'marker_shape_mismatch')
            x = [row.split() for row in lines[1:m + 1]]
        x = [[allocation_integer(q) for q in row] for row in x]
        errors = allocation_errors(x, case['supply'], case['demand'])
        if errors:
            return result | dict(status=';'.join(errors))
        cost = total_cost(x, case['costs'])
        if spec['method'] == 'VRB':
            require(obj.get('valid') is True and obj.get('cost') == cost, 'vrb_output_inconsistent')
        sparse = [[i, j, q] for i, row in enumerate(x) for j, q in enumerate(row) if q]
        return result | dict(valid=True, status='valid', cost=cost, allocation_hash=json_sha(sparse))
    except (ValueError, IndexError, TypeError, AttributeError, KeyError, ArithmeticError):
        return result | dict(status='parse_or_validation_error')


def compare(vrb, other):
    if vrb is None or other is None:
        return None
    return 'menang' if vrb < other else 'kalah' if vrb > other else 'seri'


def add_metrics(records):
    by_case = {}
    for record in records:
        group = by_case.setdefault(record['case_id'], {})
        require(record['method'] not in group, 'Rekaman metode dan kasus terduplikasi')
        group[record['method']] = record
    for group in by_case.values():
        vrb = group['VRB']
        for row in group.values():
            z, opt = row['cost'], row['optimum']
            if row['valid'] and z < opt:
                raise ValueError(f"Biaya di bawah optimum tersertifikasi: {row['case_id']} {row['method']}")
            row['gap_pct'] = 100 * (z - opt) / opt if row['valid'] and opt > 0 else None
            row['optimal'] = z == opt if row['valid'] else None
            pair = vrb['valid'] and row['valid'] and row['method'] != 'VRB'
            row['cost_vrb'] = compare(vrb['cost'], z) if pair else None
            row['runtime_vrb'] = compare(vrb['median_ns'], row['median_ns']) if pair else None


def run_case(ref, methods, rounds, work):
    case = read_case(ref)
    write_case(work / 'input.txt', case)
    rng = random.Random(int(json_sha([SEED, ref['case_id']])[:16], 16))
    order = list(methods)
    rng.shuffle(order)
    commands = {s['method']: [str(s['binary']), *('input.txt' if a == '{input}' else a for a in s['args'])] for s in methods}
    results = {}
    environment = os.environ | {'LC_ALL': 'C'}
    for spec in order:
        name = spec['method']
        try:
            completed = subprocess.run(commands[name], cwd=work, env=environment, capture_output=True, text=True, timeout=TIMEOUT)
            result = validate_output(spec, completed, case)
        except subprocess.TimeoutExpired:
            result = dict(valid=False, status='timeout', cost=None, returncode=None, allocation_hash=None)
        result['rounds_ns'] = []
        results[name] = result
    # Only launch commands that completed warm-up. Use blocking wait rather
    # than Python's timeout polling inside the timer, as in the archived run.
    with open(os.devnull, 'wb') as sink:
        for _ in range(rounds):
            rng.shuffle(order)
            for spec in order:
                name = spec['method']
                result = results[name]
                if result['returncode'] is None:
                    continue
                command = commands[name]
                start = time.perf_counter_ns()
                completed = subprocess.run(command, cwd=work, env=environment, stdout=sink, stderr=sink)
                elapsed = time.perf_counter_ns() - start
                require(completed.returncode == result['returncode'], f'Hasil proses tidak stabil: {name}, {ref["case_id"]}')
                result['rounds_ns'].append(elapsed)
    common = {k: ref[k] for k in ('case_id', 'path', 'group', 'family', 'size_bin', 'split', 'model_hash', 'optimum')}
    common.update(rows=len(case['supply']), columns=len(case['demand']))
    records = []
    for spec in methods:
        name = spec['method']
        result = results[name]
        timings = result['rounds_ns']
        records.append(common | result | dict(method=name, median_ns=statistics.median(timings) if timings else None))
    return records


def summarize(records, roster):
    rows = []
    for method in roster:
        current = [x for x in records if x['method'] == method]
        valid = [x for x in current if x['valid']]
        times = [x['median_ns'] / 1_000_000 for x in valid if x['median_ns'] is not None]
        gaps = [x['gap_pct'] for x in valid if x['gap_pct'] is not None]
        row = dict(method=method, cases=len(current), valid=len(valid), invalid=len(current)-len(valid),
                   optimal=sum(x['optimal'] for x in valid), gap_n=len(gaps),
                   mean_gap_pct=statistics.mean(gaps) if gaps else None,
                   runtime_n=len(times), mean_ms=statistics.mean(times) if times else None,
                   median_ms=statistics.median(times) if times else None,
                   min_ms=min(times) if times else None, max_ms=max(times) if times else None)
        for short, field in [('cost', 'cost_vrb'), ('runtime', 'runtime_vrb')]:
            outcomes = [x[field] for x in current if x[field] is not None]
            row[f'{short}_pairs'] = len(outcomes)
            row[f'{short}_excluded'] = len(current)-len(outcomes) if method != 'VRB' else 0
            for label in ('menang', 'seri', 'kalah'):
                row[f'{short}_{label}'] = outcomes.count(label)
        rows.append(row)
    return rows


def write_csv(path, rows, fields=None):
    with path.open('w', newline='', encoding='utf-8') as f:
        writer = csv.DictWriter(f, fieldnames=fields or list(rows[0]))
        writer.writeheader()
        for row in rows:
            writer.writerow({k: json.dumps(v, separators=(',', ':')) if isinstance(v, list) else v for k, v in row.items()})


def write_json(path, obj):
    path.write_text(json.dumps(obj, ensure_ascii=False, indent=2) + '\n')


def write_report(path, rows):
    def number(value, digits=4):
        return f'{value:.{digits}f}'.replace('.', ',') if value is not None else 'n/a'

    def wtl(row, prefix):
        return '/'.join(str(row[f'{prefix}_{s}']) for s in ('menang', 'seri', 'kalah')) if row['method'] != 'VRB' else '-'

    out = ['# Benchmark VRB pada dataset 69', '',
           'Menang, seri, dan kalah dihitung dari sudut pandang VRB pada pasangan hasil yang layak. Tanda hubung pada VRB menunjukkan baris acuan.', '',
           '## Kualitas solusi', '',
           'Gap rata-rata hanya memakai kasus layak dengan optimum positif.', '',
           '| Metode | Layak | Gap rata-rata (%) | Optimal | Menang/seri/kalah VRB | Pasangan |',
           '|---|---:|---:|---:|---:|---:|']
    for row in rows:
        out.append(f"| {row['method']} | {row['valid']}/{row['cases']} | {number(row['mean_gap_pct'], 6)} | {row['optimal']} | {wtl(row, 'cost')} | {row['cost_pairs'] if row['method'] != 'VRB' else '-'} |")
    out += ['', '## Waktu program lengkap', '',
            'Ringkasan waktu memakai median pengulangan tiap kasus layak. Seri berarti median nanodetiknya identik.', '',
            '| Metode | Median (ms) | Mean (ms) | Min (ms) | Max (ms) | Menang/seri/kalah VRB | Pasangan |',
            '|---|---:|---:|---:|---:|---:|---:|']
    for row in rows:
        times = ' | '.join(number(row[f'{k}_ms']) for k in ('median', 'mean', 'min', 'max'))
        out.append(f"| {row['method']} | {times} | {wtl(row, 'runtime')} | {row['runtime_pairs'] if row['method'] != 'VRB' else '-'} |")
    path.write_text('\n'.join(out) + '\n')


def main():
    parser = argparse.ArgumentParser(description='Uji VRB dan pembanding pada dataset 69')
    parser.add_argument('--output', type=Path, default=ROOT / 'results/69')
    parser.add_argument('--rounds', type=int, default=3)
    parser.add_argument('--case', metavar='NAMA', help='Uji satu berkas, misalnya G3_N33.txt')
    args = parser.parse_args()
    if args.rounds < 1 or args.rounds % 2 == 0:
        parser.error('--rounds harus ganjil dan positif')
    if args.case and args.output.resolve() == ROOT / 'results/69':
        parser.error('Gunakan --output berbeda untuk satu kasus')
    refs = [json.loads(line) for line in REFERENCES.read_text().splitlines() if line]
    require(len(refs) == 69 and len({r['case_id'] for r in refs}) == 69, 'Daftar referensi harus berisi 69 kasus berbeda')
    if args.case:
        refs = [r for r in refs if Path(r['path']).name == args.case]
        if not refs:
            parser.error('Kasus tidak ada dalam dataset 69')
    methods = build_methods(ROOT / 'build/benchmark')
    cpu = None
    if hasattr(os, 'sched_getaffinity') and hasattr(os, 'sched_setaffinity'):
        cpu = min(os.sched_getaffinity(0))
        os.sched_setaffinity(0, {cpu})
    random.Random(SEED).shuffle(refs)
    records = []
    with tempfile.TemporaryDirectory(prefix='vrb-benchmark-') as folder:
        for i, ref in enumerate(refs, 1):
            # One temporary directory per case isolates legacy auxiliary files.
            work = Path(folder) / str(i)
            work.mkdir()
            records.extend(run_case(ref, methods, args.rounds, work))
            if i % 10 == 0 or i == len(refs):
                print(f'{i}/{len(refs)} kasus selesai', flush=True)
    add_metrics(records)
    roster = [s['method'] for s in methods]
    require(all(r['valid'] for r in records if r['method'] == 'VRB'), 'VRB gagal pada kasus referensi')
    summary = summarize(records, roster)
    cpuinfo = Path('/proc/cpuinfo')
    model = next((line.split(':', 1)[1].strip() for line in cpuinfo.read_text().splitlines() if line.startswith('model name')), None) if cpuinfo.exists() else None
    args.output.mkdir(parents=True, exist_ok=True)
    write_csv(args.output / 'cases.csv', sorted(records, key=lambda x: (x['case_id'], roster.index(x['method']))), FIELDS)
    write_csv(args.output / 'summary.csv', summary)
    write_json(args.output / 'summary.json', summary)
    write_report(args.output / 'report.md', summary)
    write_json(args.output / 'protocol.json', dict(
        dataset='69', cases=len(refs), methods=len(methods), vrb='implementasi C modular',
        rounds=args.rounds, untimed_warmups=1, seed=SEED, cpu=cpu, cpu_model=model, platform=platform.platform(),
        gcc=subprocess.check_output(['gcc', '--version'], text=True).splitlines()[0], vrb_flags=VRB_FLAGS,
        gpp=subprocess.check_output(['g++', '--version'], text=True).splitlines()[0],
        boundary='Waktu dinding program lengkap: memulai proses, membaca file, menghitung solusi, memformat keluaran, menulis file bantu jika ada, dan menyelesaikan proses.',
        input_normalization='Semua metode menerima kasus yang sama setelah dummy biaya nol dan pembuangan marginal nol. Urutan indeks aktif tidak berubah.',
        summary='Median pengulangan per kasus, lalu median, mean, min, dan max dari median kasus layak. Gap adalah rata-rata gap kasus layak dengan optimum positif.',
        pairs='Dari sudut pandang VRB, pada pasangan kasus yang kedua alokasinya layak. Waktu sama jika median nanodetik identik.',
        runner_sha256=sha(Path(__file__)), reference_sha256=sha(REFERENCES), roster_sha256=sha(ROSTER),
        binaries={spec['method']: spec['binary_sha256'] for spec in methods},
    ))
    print(f'Hasil disimpan di {args.output}')


if __name__ == '__main__':
    main()
