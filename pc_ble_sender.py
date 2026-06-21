"""
PC BLE sender for ESP32 stats monitor.

Sends telemetry to ESP32 over BLE characteristic every SEND_INTERVAL seconds.
Compatible with firmware UUIDs:
  service:        9f1f8f00-1d7d-4b4f-9da8-f2f89d9ef001
  characteristic: 9f1f8f01-1d7d-4b4f-9da8-f2f89d9ef001

Payload example:
  {
    "cpu": 23.4,
    "gpu_used": 41.2,
        "gpu_temp": 62.5,
    "ram_used": 12.7,
    "ram_total": 31.8,
        "datetime": "01/01/2026 10:01:00"
  }

Install:
  pip install bleak psutil
"""

import asyncio
import json
import os
import signal
import subprocess
import sys
import time
from datetime import datetime
from typing import Optional

import psutil
from bleak import BleakClient, BleakScanner

DEVICE_NAME = "ESP32-Stats-Monitor"
SERVICE_UUID = "9f1f8f00-1d7d-4b4f-9da8-f2f89d9ef001"
CHAR_UUID = "9f1f8f01-1d7d-4b4f-9da8-f2f89d9ef001"
DEVICE_ADDRESS = os.getenv("ESP32_BLE_ADDRESS", "").strip()  # optional override
SEND_INTERVAL = 0.2
SCAN_TIMEOUT = 6.0
WRITE_WITH_RESPONSE = False
DATETIME_FORMAT = "%d/%m/%Y %H:%M:%S"
MAX_BLE_PAYLOAD_BYTES = 220

_HAS_NVIDIA = False

_CPU_NAME: str = ""
_GPU_NAME: str = ""
_WIFI_NAME: str = ""


def log(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] {msg}")


def get_current_datetime_str() -> str:
    """Return local datetime string for ESP32 footer (dd/MM/yyyy HH:mm:ss)."""
    return datetime.now().strftime(DATETIME_FORMAT)


def _encode_payload(data: dict) -> bytes:
    return json.dumps(data, separators=(",", ":")).encode("utf-8")


def _get_cpu_model() -> str:
    """Cache and return CPU model name via wmic (Windows only)."""
    try:
        r = subprocess.run(
            ["wmic", "cpu", "get", "name"],
            capture_output=True,
            text=True,
            timeout=3,
        )
        if r.returncode != 0:
            return ""
        lines = [l.strip() for l in r.stdout.splitlines() if l.strip()]
        for line in lines:
            if line.lower() != "name" and line:
                return line
        return ""
    except Exception:
        return ""


def _get_gpu_model() -> str:
    """Cache and return GPU model name via nvidia-smi."""
    if not _HAS_NVIDIA:
        return ""
    try:
        r = subprocess.run(
            ["nvidia-smi", "--query-gpu=name", "--format=csv,noheader"],
            capture_output=True,
            text=True,
            timeout=3,
        )
        if r.returncode != 0:
            return ""
        return r.stdout.strip().splitlines()[0].strip()
    except Exception:
        return ""


def _get_wifi_ssid() -> str:
    """Get connected WiFi SSID (Windows via netsh).
    
    Checks interface connection state before extracting SSID to avoid
    returning SSID from disconnected adapters.
    """
    try:
        r = subprocess.run(
            ["netsh", "wlan", "show", "interfaces"],
            capture_output=True,
            text=True,
            timeout=2,
        )
        if r.returncode != 0 or not r.stdout.strip():
            return ""
        
        lines = r.stdout.splitlines()
        interface_connected = False
        ssid_value = ""
        
        for line in lines:
            # Check if interface state is "connected"
            if "State" in line and ":" in line:
                state = line.split(":", 1)[1].strip().lower()
                interface_connected = "connected" in state
            
            # Extract SSID only if we've confirmed interface is connected
            if "SSID" in line and ":" in line and not interface_connected:
                # Skip until we find State field
                continue
            
            if "SSID" in line and ":" in line and interface_connected:
                parts = line.split(":", 1)
                if len(parts) == 2:
                    ssid = parts[1].strip()
                    # Reject None, disabled, empty values
                    if ssid and ssid.lower() not in ("none", "disabled", ""):
                        return ssid
        
        return ""
    except Exception:
        return ""


def _check_nvidia() -> bool:
    try:
        r = subprocess.run(
            ["nvidia-smi", "--query-gpu=count", "--format=csv,noheader,nounits"],
            capture_output=True,
            text=True,
            timeout=2,
        )
        return r.returncode == 0 and r.stdout.strip().isdigit() and int(r.stdout.strip()) > 0
    except Exception:
        return False


def get_cpu_percent() -> float:
    return round(psutil.cpu_percent(interval=None), 1)


def get_ram_used_total_gb() -> tuple[float, float]:
    vm = psutil.virtual_memory()
    used_gb = vm.used / (1024.0 ** 3)
    total_gb = vm.total / (1024.0 ** 3)
    return round(used_gb, 2), round(total_gb, 2)


def get_gpu_metrics() -> tuple[float, float]:
    if not _HAS_NVIDIA:
        return 0.0, 0.0
    try:
        r = subprocess.run(
            [
                "nvidia-smi",
                "--query-gpu=utilization.gpu,temperature.gpu",
                "--format=csv,noheader,nounits",
            ],
            capture_output=True,
            text=True,
            timeout=2,
        )
        if r.returncode != 0:
            return 0.0, 0.0
        line = r.stdout.strip().splitlines()[0].strip()
        parts = [p.strip() for p in line.split(",")]
        if len(parts) < 2:
            return 0.0, 0.0
        usage = max(0.0, min(100.0, float(parts[0])))
        temp = max(0.0, float(parts[1]))
        return usage, temp
    except Exception:
        return 0.0, 0.0


def get_net_speed_kbps() -> tuple[float, float]:
    """Return (upload_kbps, download_kbps). Zeroes if first call."""
    if not hasattr(get_net_speed_kbps, "_last_io"):
        get_net_speed_kbps._last_io = psutil.net_io_counters()
        get_net_speed_kbps._last_time = time.monotonic()
        return 0.0, 0.0

    now = time.monotonic()
    dt = now - get_net_speed_kbps._last_time
    if dt <= 0.0:
        return 0.0, 0.0

    cur = psutil.net_io_counters()
    sent_kb = (cur.bytes_sent - get_net_speed_kbps._last_io.bytes_sent) / 1024.0
    recv_kb = (cur.bytes_recv - get_net_speed_kbps._last_io.bytes_recv) / 1024.0

    get_net_speed_kbps._last_io = cur
    get_net_speed_kbps._last_time = now

    return sent_kb / dt, recv_kb / dt


def build_payload_part(send_identity_part: bool) -> tuple[bytes, str, str]:
    ram_used, ram_total = get_ram_used_total_gb()
    gpu_used, gpu_temp = get_gpu_metrics()
    net_up, net_down = get_net_speed_kbps()
    datetime_str = get_current_datetime_str()

    metrics_data = {
        "cpu": get_cpu_percent(),
        "gpu_used": round(gpu_used, 1),
        "gpu_temp": round(gpu_temp, 1),
        "ram_used": ram_used,
        "ram_total": ram_total,
        "net_up": round(net_up, 1),
        "net_down": round(net_down, 1),
    }

    if not send_identity_part:
        # Part A: fast-changing numeric metrics (no datetime, already in identity).
        payload = _encode_payload(metrics_data)
        if len(payload) <= MAX_BLE_PAYLOAD_BYTES:
            return payload, datetime_str, "metrics"

        # Fallback for very small ATT payload limits.
        no_net_down = dict(metrics_data)
        no_net_down.pop("net_down", None)
        return _encode_payload(no_net_down), datetime_str, "metrics-min"

    # Part B: slower-changing identity fields, including wifi_name.
    identity_data = {
        "cpu_name": _CPU_NAME,
        "gpu_name": _GPU_NAME,
        "wifi_name": _WIFI_NAME,
        "datetime": datetime_str,
    }
    payload = _encode_payload(identity_data)
    if len(payload) <= MAX_BLE_PAYLOAD_BYTES:
        return payload, datetime_str, "identity"

    wifi_only_data = {
        "wifi_name": _WIFI_NAME,
        "datetime": datetime_str,
    }
    payload = _encode_payload(wifi_only_data)
    if len(payload) <= MAX_BLE_PAYLOAD_BYTES:
        return payload, datetime_str, "identity-wifi"

    return _encode_payload({"wifi_name": _WIFI_NAME}), datetime_str, "identity-min"


async def find_device_address() -> Optional[str]:
    if DEVICE_ADDRESS:
        log(f"Using fixed BLE address from ESP32_BLE_ADDRESS: {DEVICE_ADDRESS}")
        return DEVICE_ADDRESS

    log(f"Scanning BLE for name '{DEVICE_NAME}' or service '{SERVICE_UUID}' ...")

    # return_adv=True gives advertisement metadata; local_name/service_uuids are often
    # more reliable than d.name on Windows.
    devices = await BleakScanner.discover(timeout=SCAN_TIMEOUT, return_adv=True)

    for _, (device, adv) in devices.items():
        local_name = adv.local_name or device.name or ""
        service_uuids = [u.lower() for u in (adv.service_uuids or [])]

        if local_name == DEVICE_NAME or SERVICE_UUID.lower() in service_uuids:
            return device.address

    # Diagnostic output to help identify what Windows BLE stack can see.
    if devices:
        log("Nearby BLE devices:")
        for _, (device, adv) in devices.items():
            local_name = adv.local_name or device.name or "(no-name)"
            log(f"  - {local_name} [{device.address}]")
    else:
        log("No BLE advertisements detected in this scan window")

    return None


async def send_loop(address: str) -> None:
    log(f"Connecting to {address} ...")
    async with BleakClient(address, timeout=20.0) as client:
        if not client.is_connected:
            raise RuntimeError("BLE connect failed")

        # Bleak API differs across versions. In newer versions, use client.services.
        services = getattr(client, "services", None)
        if services is not None:
            try:
                if not services.get_service(SERVICE_UUID):
                    log("Warning: service UUID not found, will still try characteristic write")
            except Exception:
                log("Warning: unable to inspect services, will still try characteristic write")

        log("Connected. Streaming telemetry in 2 parts (metrics/identity)...")
        send_identity_part = False
        while True:
            payload, datetime_str, part = build_payload_part(send_identity_part)
            log(f"Sending {len(payload)} bytes ({part}), datetime={datetime_str}")
            await client.write_gatt_char(CHAR_UUID, payload, response=WRITE_WITH_RESPONSE)
            send_identity_part = not send_identity_part
            await asyncio.sleep(SEND_INTERVAL)


async def main() -> None:
    global _HAS_NVIDIA, _CPU_NAME, _GPU_NAME, _WIFI_NAME
    _HAS_NVIDIA = _check_nvidia()
    _CPU_NAME = _get_cpu_model()
    _GPU_NAME = _get_gpu_model()
    _WIFI_NAME = _get_wifi_ssid()
    log(f"CPU: {_CPU_NAME}")
    log(f"GPU: {_GPU_NAME}")
    log(f"WiFi: {_WIFI_NAME}")

    stop = asyncio.Event()

    def _stop(*_args):
        stop.set()

    signal.signal(signal.SIGINT, _stop)
    signal.signal(signal.SIGTERM, _stop)

    while not stop.is_set():
        try:
            address = await find_device_address()
            if not address:
                log("Device not found. Retry in 2s")
                await asyncio.sleep(2.0)
                continue

            await send_loop(address)
        except asyncio.CancelledError:
            break
        except Exception as e:
            log(f"Disconnected/Error: {e}")
            await asyncio.sleep(1.5)


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
    except Exception as ex:
        log(f"Fatal error: {ex}")
        sys.exit(1)
