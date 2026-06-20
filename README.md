# GpsApp

GPS fix logger and publisher for the OpenRFStack ecosystem. Reads fixes from
`gpsd`, writes them to a JSON-lines log, and publishes them over AMQP so
other nodes (`mobile-node`, `recon-node`) can GPS-tag detections and IQ
captures.

## Architecture

```
gpsd (standard Linux GPS daemon)
     │
     ▼
GpsdClient ──poll──► GpsFix
     │
     ├──► GpsLogger        (append JSON-lines to fix_log)
     └──► AmqpPublisher    (publish GPS_FIX JSON to broker topic, rate-limited
                             by publish_interval_ms)
```

## Configuration

`config/gps.xml` (override at runtime with
`-v /path/to/gps.xml:/etc/sdr-gps/gps.xml:ro,z`):

- `broker` — Artemis AMQP URL/credentials and the topic GPS_FIX messages are
  published to (`gps.location` by default).
- `gpsd` — host/port of the gpsd instance to poll (`localhost:2947` by
  default).
- `logging` — JSON-lines fix log path, publish interval, minimum fix mode
  (2D/3D) required before a fix is considered valid, and the `device_id`
  stamped on every published message.

## Building

```
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Dependencies: `qpid-proton-cpp`, `tinyxml2`, `spdlog`, `fmt`,
`nlohmann_json` (fetched automatically if not found on the system).

## Running

```
./sdr_gps /etc/sdr-gps/gps.xml
```

Requires a running `gpsd` instance and a reachable Artemis broker.
