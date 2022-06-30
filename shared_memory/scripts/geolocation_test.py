#!/usr/bin/python3

import subprocess
import time
import argparse


class TestData:
    def __init__(self, ip, expected):
        self.ip = ip
        self.expected = expected


# Fail gracefully if psutil package is not installed
try:
    import psutil
except ImportError:
    print("'psutil' not found, install with:\n> python3 -m pip install psutil")
    exit(1)


def get_memory_usage(process):
    process = psutil.Process(process.pid)
    return process.memory_info().rss


def format_memory_usage(rss):
    KB = 1024
    MB = 1024 * 1024
    GB = 1024 * 1024 * 1024

    if rss >= GB:
        return f'{round(rss / GB, 2)}gb'
    elif rss >= MB:
        return f'{round(rss / MB, 2)}mb'
    elif rss >= KB:
        return f'{round(rss / KB, 2)}kb'
    else:
        return f'{round(rss, 2)}b'


def format_time(nanoseconds):
    if nanoseconds < 1000:
        return f'{int(nanoseconds)}ns'
    elif nanoseconds < 1000000:
        return f'{int(nanoseconds / 1000)}μs'
    elif nanoseconds < 1000000000:
        return f'{int(nanoseconds / 1000000)}ms'
    else:
        return f'{int(nanoseconds / 1000000000)}s'


def wait_ready(process):
    """Wait for process under test to indicate readyness"""
    resp = process.stdout.readline()
    if resp.strip() != "READY":
        raise Exception("The app failed to indicate readyness")


def send_command(process, command):
    """Send command to the process under test and measure processing time"""
    start = time.time_ns()
    process.stdin.write(command + "\n")
    result = process.stdout.readline()
    end = time.time_ns()
    duration = (end - start)

    memory_usage = get_memory_usage(process)

    return (result, duration, memory_usage)


def send_load_command(process):
    """Send command to the process under test to load the database"""
    result, time, memory_usage = send_command(process, "LOAD")
    if result.strip() != "OK":
        raise Exception("The app failed to load the database")
    return (time, memory_usage)


def send_exit_command(process):
    """Send exit command to the process under test"""
    send_command(process, "EXIT")


def send_lookup_command(process, ip):
    """Send command to the process under test to perform geolocation loookup"""
    answer, time, memory_usage = send_command(process, "LOOKUP " + ip)
    return (answer.strip(), time, memory_usage)


if __name__ == "__main__":
    # Parse CLI arguments
    parser = argparse.ArgumentParser(description="An utility implementing "
                                     "line-based request-response "
                                     "protocol for perfoming "
                                     "geolocation lookups")
    parser.add_argument("--executable",
                        help="An executable file performing lookups",
                        required=True)
    parser.add_argument("--database",
                        help="A path to database file",
                        required=True)
    args = parser.parse_args()

    # Start the process
    process = subprocess.Popen(
        [args.executable, args.database],
        bufsize=1,
        universal_newlines=True,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE)
    wait_ready(process)

    # Request to load the database
    load_latency, memory_usage = send_load_command(process)
    print("Database loaded, Memory usage: %s, Load time: %s" %
            (format_memory_usage(memory_usage), format_time(load_latency)))

    # Perform some tests
    test_suite = [
        TestData('0.0.0.0',         '-,-'),
        TestData('0.1.0.0',         '-,-'),
        TestData('0.255.255.255',   '-,-'),
        TestData('1.0.0.0',         'US,Los Angeles'),
        TestData('1.2.3.4',         'AU,Brisbane'),
        TestData('5.44.16.0',       'GB,Hastings'),
        TestData('8.8.8.8',         'US,Mountain View'),
        TestData('8.24.99.0',       'US,Hastings'),
        TestData('10.0.0.0',        '-,-'),
        TestData('53.103.143.255',  'US,Des Moines'),
        TestData('53.103.144.0',    'DE,Stuttgart'),
        TestData('53.103.255.255',  'DE,Stuttgart'),
        TestData('53.104.0.0',      'DE,Stuttgart'),
        TestData('53.255.255.255',  'DE,Stuttgart'),
        TestData('54.0.0.0',        'US,Rahway'),
        TestData('71.6.28.0',       'US,San Jose'),
        TestData('71.6.28.255',     'US,San Jose'),
        TestData('71.6.29.0',       'US,Concord'),
        TestData('79.238.202.1',    'DE,Beverstedt, Flecken'),
        TestData('197.211.217.7',   'ZW,Victoria Falls'),
        TestData('223.255.255.255', 'AU,Brisbane'),
        TestData('255.255.255.255', '-,-'),
    ]
    print('%-4s %-15s %-30s %-8s %-8s %-6s' % ('', 'IP', 'ANSWER', 'MEMORY', 'LOOKUP', 'POINTS') )
    for test in test_suite:
        answer, lookup_time, memory_usage = send_lookup_command(process, test.ip)
        correct = (answer == test.expected)

        if correct:
            load_time_ms = load_latency / 1000000
            memory_usage_mb = memory_usage / (1024*1024)
            lookup_time_ms = lookup_time / 1000000
            points = load_time_ms + memory_usage_mb * 10 + lookup_time_ms * 1000

            print('\x1b[1;32mOK\x1b[0m   %-15s %-30s %-8s %-8s %6.1f' % (
                  test.ip, answer,
                  format_memory_usage(memory_usage), format_time(lookup_time), points))
        else:
            print('\x1b[1;31mFAIL %-15s %s ("%s" expected)\x1b[0m' % (
                  test.ip, answer, test.expected))
            continue

    send_exit_command(process)
    process.wait()
