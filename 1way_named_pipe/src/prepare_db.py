#!/usr/bin/python3

import csv
import numpy
import ipaddress

HALFIPS_COUNT    = 256*256 # All combinations of 2 first octets of IP address
HALFIP_DATA_SIZE = 4+4     #4B: octets 1 & 2 of end IP, 4B: offset in BLOCK2 segment
BLOCK2_DATA_SIZE = 1+3     #1B: octet 3 of end IP,      3B: offset in CITY segment

HALFIPS_SEGMENT_SIZE = HALFIP_DATA_SIZE*HALFIPS_COUNT # could be "3+3" => -0.1MB => not worth

db_path = './database.csv'
db = []
city2offset = {}
ip2city = []
halfips = [[-1,-1]]*HALFIPS_COUNT

print("Loading csv...")
with open(db_path, newline='') as csvfile:
    csvreader = csv.reader(csvfile)
    for nr, row in enumerate(csvreader):
        db.append(row)

print("Preparing SEG3 - 0-teminated city strings...")
cities = [ (rec[2]+","+rec[5]) for rec in db ]
cities = numpy.unique(sorted(cities))
offset = 0
for city in cities:
    city2offset[city] = offset
    offset += len(city) + 1 # '+1' - ends with '\0' symbol
cities_segment_size = offset

print("Preparing SEG2 - map: Octet3 => SEG3 offset (to city)...")
for nr, rec in enumerate(db):
    ip_end       = int(rec[1])
    country_city = rec[2]+","+rec[5]
    city_offset  = city2offset[country_city]
    this_offset  = nr * BLOCK2_DATA_SIZE
    a1,a2,a3,a4  = (ip_end & 0xFFFFFFFF).to_bytes(4, 'big')
    halfip       = int.from_bytes([a1, a2], 'big')
    if halfips[halfip][0] == -1:
        halfips[halfip] = [this_offset, -2]
    ip2city.append([ip_end, city_offset, this_offset, halfip, a1, a2, a3, a4])

print("Preparing SEG1 - map: Halfip => SEG2 offset (to search block)...")
cities_count = len(cities)
ip2city_count = len(ip2city)
ip2city_segment_size = BLOCK2_DATA_SIZE*ip2city_count
total_size = ip2city_segment_size +  HALFIPS_SEGMENT_SIZE + cities_segment_size
use_offset = ip2city_segment_size #a la previous offset
use_count = ip2city_count
for halfip in range(HALFIPS_COUNT-1, -1, -1):
    offset = halfips[halfip][0]
    if offset != -1:
        use_count = (use_offset - offset)//BLOCK2_DATA_SIZE
        use_offset = offset
    halfips[halfip] = [use_offset, use_count]

print("=============================")
print("Records:        %9d" % (len(db)))
print("SEG0 (jumps):   %9d (1 record)" % (4))
print("SEG1 (halfips): %9d %6.0fK (%d records)" % (HALFIPS_SEGMENT_SIZE, HALFIPS_SEGMENT_SIZE/1024, HALFIPS_COUNT))
print("SEG2 (ip2city): %9d %6.0fK (%d records)" % (ip2city_segment_size, ip2city_segment_size/1024, ip2city_count))
print("SEG3 (cities):  %9d %6.0fK (%d records)" % (cities_segment_size, cities_segment_size/1024, cities_count))
print("TOTAL SIZE:     %9d %6.0fK %3.1fM" % (total_size, total_size/1024, total_size/1024/1024))
# Countries: 241
# City count per Country - one:28, 1b:175,2b:38, 3b:203

with open("geo.db", "wb") as out:
    # SEG0
    SEG3_START = 4 + HALFIPS_SEGMENT_SIZE + ip2city_segment_size # '+4' => add self
    out.write(SEG3_START.to_bytes(4, byteorder='little'))

    # SEG1
    for halfip, block in enumerate(halfips):
        offset = int(block[0])
        record_count = int(block[1])
        out.write(offset.to_bytes(4, byteorder='little'))
        out.write(record_count.to_bytes(4, byteorder='little'))

    # SEG2
    for ip_end, city_offset, offset, halfip, a1, a2, a3, a4 in ip2city:
        IP = ipaddress.ip_address(ip_end).__str__()
        _, _, octet3, _ = IP.split('.')
        octet3 = int(octet3)
        out.write(octet3.to_bytes(1, byteorder='little'))
        out.write(city_offset.to_bytes(3, byteorder='little'))

    # SEG3
    for city, _offset in city2offset.items():
        out.write(city.encode('ascii'))
        out.write("\0".encode('ascii'))
