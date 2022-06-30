#!/usr/bin/python3

import subprocess
import time
import argparse
import csv
import random
import socket
import struct


# Fail gracefully if psutil package is not installed
try:
    import psutil
except ImportError:
    print("'psutil' not found, install with:\n> python3 -m pip install psutil")
    exit(1)

def int2ip(addr):
    return socket.inet_ntoa(struct.pack("!I", int(addr)))

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
    if nanoseconds < 10000:
        return f'{round(nanoseconds, 2)}ns'
    elif nanoseconds < 10000000:
        return f'{round(nanoseconds / 1000, 2)}μs'
    elif nanoseconds < 10000000000:
        return f'{round(nanoseconds / 1000000, 2)}ms'
    else:
        return f'{round(nanoseconds / 1000000000, 2)}s'


def wait_ready(process):
    """Wait for process under test to indicate readyness"""
    resp = process.stdout.readline()
    if resp.strip() != "READY":
        raise Exception("The app failed to indicate readyness")


def send_command(process, command):
    """Send command to the process under test and measure processing time"""
    start = time.time_ns()
    command = command + "\n"
    process.stdin.write(command)
    result = process.stdout.readline()
    end = time.time_ns()

    memory_usage = get_memory_usage(process)

    return (result, end - start, memory_usage)


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


class TestData:
    def __init__(self, ip, expected):
        self.ip = ip
        self.expected = expected

    def __str__(self):
        return str(self.ip) + " => " + self.expected


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
                        help="A path to preprocessed database file",
                        required=True)
    parser.add_argument("--original-database",
                        help="A path to original database file",
                        required=True)
    args = parser.parse_args()

    # Start the process
    print("Starting process...")
    process = subprocess.Popen(
        [args.executable, args.database],
        bufsize=1,
        universal_newlines=True,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE)

    print("Waiting for process to be ready...")
    wait_ready(process)

    # Request to load the database
    print("Loading database...")
    load_latency, load_memory_usage = send_load_command(process)
    print("Database loaded, Memory usage: %s, Load time: %s" %
            (format_memory_usage(load_memory_usage), format_time(load_latency)))

    print("Loading CSV...")
    entries = []
    with open(args.original_database, newline='') as csvfile:
        csvreader = csv.reader(csvfile, delimiter=',', quotechar='"')
        for row in csvreader:
            entries.append(
                TestData(
                    str(int(row[0]) + random.randint(0, 255)),
                    row[2]+","+row[5]))

    print("Randomizing CSV...")
    random.shuffle(entries)

    print("Running Tests")
    sum_latency = 0
    max_memory_usage = 0
    last_percentage_done = -1
    failures_cnt = 0
    incorrect_cnt = 0
    correct_cnt = 0
    last_failure = None
    for idx, entry in enumerate(entries):
        # execute test
        try:
            answer, latency_ns, memory_usage = send_lookup_command(process,
                                                                   int2ip(entry.ip))
            # check the answer
            # if answer != entry.expected:
            #     print(entry.ip, answer + " != " + entry.expected)
            #     incorrect_cnt += 1
            #     continue


        except Exception as e:
            if last_failure is None or last_failure == e:
                print("failure: ", e)
                last_failure = e
            failures_cnt += 1
            continue

        correct_cnt += 1

        # Gather stats
        sum_latency += latency_ns
        if max_memory_usage < memory_usage:
            max_memory_usage = memory_usage


        # Report progress
        percentage_done = round(idx / len(entries) * 100, 0)
        if percentage_done != last_percentage_done:
            load_time_ms = load_latency / 1000000
            max_memory_usage_mb = max_memory_usage / (1024*1024)
            avg_lookup_time = sum_latency / correct_cnt
            avg_lookup_time_ms = avg_lookup_time / 1000000
            points = load_time_ms + max_memory_usage_mb * 10 + avg_lookup_time_ms * 1000
            print("%.1f%%  curr.points: %.1f  curr.memory: %s  avg.lookup: %s" % (
                    percentage_done, points,
                    format_memory_usage(max_memory_usage),
                    format_time(avg_lookup_time)))
            last_percentage_done = percentage_done
        # break

    # Gather results
    load_time_ms = load_latency / 1000000
    avg_latency = sum_latency / correct_cnt
    avg_latency_ms = avg_latency / 1000000
    max_memory_usage_mb = max_memory_usage / (1024*1024)
    points = load_time_ms + max_memory_usage_mb * 10 + avg_latency_ms * 1000
    correct_percentage = round(correct_cnt / len(entries) * 100, 2)
    failures_percentage = round(failures_cnt / len(entries) * 100, 2)
    incorrect_percentage = round(incorrect_cnt / len(entries) * 100, 2)
    points = load_time_ms + max_memory_usage_mb * 10 + avg_latency_ms * 1000

    print("Results:")
    print("      Correct count: ", correct_cnt, correct_percentage, "%")
    print("    Incorrect count: ", incorrect_cnt, incorrect_percentage, "%")
    print("      Failure count: ", failures_cnt, failures_percentage, "%")
    print("       Memory usage: %-9s \t%6.2f points" % (format_memory_usage(max_memory_usage), max_memory_usage_mb * 10))
    print("       DB Load time: %-9s \t%6.2f points" % (format_time(load_latency), load_time_ms))
    print("        Lookup time: %-9s \t%6.2f points" % (format_time(avg_latency), avg_latency_ms * 1000))
    print("             Points: %5.2f" % (points))

    send_exit_command(process)
    process.wait()
