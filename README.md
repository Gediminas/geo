# Geo task

## Build

```
./prepare_db.sh
./build.sh

./geo geo.db
# or
./geo geo.db test
# or
./speed_test.sh
```

## Assumptions

* Whole IPv4 range is covered 0.0.0.0 - 255.255.255.255
* There are no gaps between ranges
* IP range first IP last octet is always x.x.x.0
* IP range last IP last octet is always x.x.x.255
