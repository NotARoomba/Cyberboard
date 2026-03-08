"""
3D cube demo driven by ICM-42688-PC + BMP580 sensor data over BLE.

Connects to the Cyberboard BLE GATT service and reads:
  - IMU_DATA characteristic (28 bytes = 7 floats: ax,ay,az, gx,gy,gz, temp)
  - BARO_DATA characteristic (12 bytes = 3 floats: pressure, temperature, reserved)

Renders a cube oriented by the gravity vector (quaternion-based, no gimbal lock),
with acceleration arrows shooting from the cube faces, and a text HUD showing
temperature, pressure, and estimated altitude.

Usage:
    python cube_demo_ble.py                  # auto-scan for "Cyberboard"
    python cube_demo_ble.py AA:BB:CC:DD:EE   # connect by address
"""

import sys
import math
import struct
import time
import asyncio
import threading
import ctypes
import pygame
from pygame.locals import (
    DOUBLEBUF, OPENGL, QUIT, KEYDOWN, K_ESCAPE,
    MOUSEBUTTONDOWN, MOUSEBUTTONUP, MOUSEMOTION, MOUSEWHEEL,
)
from OpenGL.GL import (
    glClear, glClearColor, glEnable, glDisable, glLoadIdentity, glBegin, glEnd,
    glVertex3f, glVertex3fv, glColor3f, glColor3fv, glColor4f,
    glTranslatef, glRotatef, glLineWidth, glMultMatrixf, glPushMatrix, glPopMatrix,
    glMatrixMode, glOrtho, glBlendFunc, glDepthMask,
    GL_COLOR_BUFFER_BIT, GL_DEPTH_BUFFER_BIT, GL_DEPTH_TEST,
    GL_LINES, GL_QUADS, GL_TRIANGLES, GL_BLEND, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
    GL_PROJECTION, GL_MODELVIEW, GL_TRUE, GL_FALSE,
)
from OpenGL.GLU import gluPerspective
from bleak import BleakClient, BleakScanner

# ── BLE UUIDs (must match firmware custom_stm.c) ──
# Service: 8fe5b3d5-2e7f-4a98-2a48-7acc-00000000
# The 128-bit UUIDs are stored LSB-first in the firmware COPY_UUID_128 macro.
# Firmware bytes (MSB first): 00,00,00,XX, 8e,22,45,41, 9d,4c,21,ed, ae,82,ed,19
# Standard UUID format: 19ed82ae-ed21-4c9d-4145-228e000000XX

SERVICE_UUID     = "00000000-cc7a-482a-984a-7f2ed5b3e58f"
IMU_DATA_UUID    = "00000001-8e22-4541-9d4c-21edae82ed19"
BARO_DATA_UUID   = "00000002-8e22-4541-9d4c-21edae82ed19"

DEVICE_NAME = "Cyberboard"

# Sea-level pressure for altitude estimation (Pa)
SEA_LEVEL_PA = 101325.0


# ── Cube geometry (same as serial demo) ──

VERTICES = [
    ( 1,  1, -1), ( 1, -1, -1), (-1, -1, -1), (-1,  1, -1),
    ( 1,  1,  1), ( 1, -1,  1), (-1, -1,  1), (-1,  1,  1),
]

EDGES = [
    (0, 1), (1, 2), (2, 3), (3, 0),
    (4, 5), (5, 6), (6, 7), (7, 4),
    (0, 4), (1, 5), (2, 6), (3, 7),
]

FACES = [
    (0, 1, 2, 3), (4, 5, 6, 7),
    (0, 1, 5, 4), (2, 3, 7, 6),
    (0, 3, 7, 4), (1, 2, 6, 5),
]

FACE_COLORS = [
    (0.8, 0.2, 0.2), (0.2, 0.8, 0.2),
    (0.2, 0.2, 0.8), (0.8, 0.8, 0.2),
    (0.8, 0.2, 0.8), (0.2, 0.8, 0.8),
]


def draw_cube():
    glBegin(GL_QUADS)
    for i, face in enumerate(FACES):
        glColor3fv(FACE_COLORS[i])
        for vertex in face:
            glVertex3fv(VERTICES[vertex])
    glEnd()

    glLineWidth(2.0)
    glBegin(GL_LINES)
    glColor3fv((1, 1, 1))
    for edge in EDGES:
        for vertex in edge:
            glVertex3fv(VERTICES[vertex])
    glEnd()


def draw_arrow(origin, direction, length, color):
    if length < 0.01:
        return
    ox, oy, oz = origin
    dx, dy, dz = direction
    ex = ox + dx * length
    ey = oy + dy * length
    ez = oz + dz * length

    glColor3f(*color)
    glLineWidth(3.0)
    glBegin(GL_LINES)
    glVertex3f(ox, oy, oz)
    glVertex3f(ex, ey, ez)
    glEnd()

    head_len = min(0.3, length * 0.3)
    head_r = 0.08
    if abs(dx) < 0.9:
        perp1 = (0, -dz, dy)
    else:
        perp1 = (-dz, 0, dx)
    mag = math.sqrt(perp1[0]**2 + perp1[1]**2 + perp1[2]**2)
    if mag < 1e-9:
        return
    perp1 = (perp1[0]/mag, perp1[1]/mag, perp1[2]/mag)
    perp2 = (
        dy * perp1[2] - dz * perp1[1],
        dz * perp1[0] - dx * perp1[2],
        dx * perp1[1] - dy * perp1[0],
    )
    bx = ex - dx * head_len
    by = ey - dy * head_len
    bz = ez - dz * head_len

    segments = 8
    glBegin(GL_TRIANGLES)
    for i in range(segments):
        a1 = 2 * math.pi * i / segments
        a2 = 2 * math.pi * (i + 1) / segments
        c1, s1 = math.cos(a1), math.sin(a1)
        c2, s2 = math.cos(a2), math.sin(a2)
        p1 = (
            bx + head_r * (c1 * perp1[0] + s1 * perp2[0]),
            by + head_r * (c1 * perp1[1] + s1 * perp2[1]),
            bz + head_r * (c1 * perp1[2] + s1 * perp2[2]),
        )
        p2 = (
            bx + head_r * (c2 * perp1[0] + s2 * perp2[0]),
            by + head_r * (c2 * perp1[1] + s2 * perp2[1]),
            bz + head_r * (c2 * perp1[2] + s2 * perp2[2]),
        )
        glVertex3f(ex, ey, ez)
        glVertex3f(*p1)
        glVertex3f(*p2)
    glEnd()


def draw_accel_arrows(ax, ay, az):
    scale = 2.0
    if abs(ax) > 0.01:
        sign = 1.0 if ax > 0 else -1.0
        draw_arrow((sign * 1.0, 0, 0), (sign, 0, 0), abs(ax) * scale, (1.0, 0.4, 0.4))
    if abs(ay) > 0.01:
        sign = 1.0 if ay > 0 else -1.0
        draw_arrow((0, sign * 1.0, 0), (0, sign, 0), abs(ay) * scale, (0.4, 1.0, 0.4))
    if abs(az) > 0.01:
        sign = 1.0 if az > 0 else -1.0
        draw_arrow((0, 0, sign * 1.0), (0, 0, sign), abs(az) * scale, (0.4, 0.4, 1.0))


# ── Text HUD overlay ──

def draw_text_hud(surface, font, shared):
    w, h = surface.get_size()
    surface.fill((0, 0, 0, 0))

    lines = []
    lines.append(f"Accel: {shared['ax']:+.2f}, {shared['ay']:+.2f}, {shared['az']:+.2f} g")
    lines.append(f"Gyro:  {shared['gx']:+.1f}, {shared['gy']:+.1f}, {shared['gz']:+.1f} dps")
    lines.append(f"IMU Temp: {shared['imu_temp']:.1f} C")
    lines.append("")

    pressure = shared["pressure"]
    bmp_temp = shared["bmp_temp"]
    if pressure > 0:
        altitude = 44330.0 * (1.0 - (pressure / SEA_LEVEL_PA) ** (1.0 / 5.255))
        lines.append(f"Pressure: {pressure:.0f} Pa")
        lines.append(f"Altitude: {altitude:.1f} m")
        lines.append(f"Baro Temp: {bmp_temp:.1f} C")
    else:
        lines.append("Pressure: --")
        lines.append("Altitude: --")
        lines.append("Baro Temp: --")

    lines.append("")
    lines.append(f"Transport: BLE")

    y = 10
    for line in lines:
        if line == "":
            y += 8
            continue
        text_surf = font.render(line, True, (220, 220, 220))
        surface.blit(text_surf, (10, y))
        y += text_surf.get_height() + 2

    status = "BLE CONNECTED" if shared["connected"] else "BLE SCANNING..."
    color = (100, 255, 100) if shared["connected"] else (255, 200, 100)
    status_surf = font.render(status, True, color)
    surface.blit(status_surf, (10, h - status_surf.get_height() - 10))

    text_data = pygame.image.tostring(surface, "RGBA", True)
    glMatrixMode(GL_PROJECTION)
    glPushMatrix()
    glLoadIdentity()
    glOrtho(0, w, 0, h, -1, 1)
    glMatrixMode(GL_MODELVIEW)
    glPushMatrix()
    glLoadIdentity()
    glDisable(GL_DEPTH_TEST)
    glEnable(GL_BLEND)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
    from OpenGL.GL import glRasterPos2i, glDrawPixels, GL_RGBA, GL_UNSIGNED_BYTE
    glRasterPos2i(0, 0)
    glDrawPixels(w, h, GL_RGBA, GL_UNSIGNED_BYTE, text_data)
    glDisable(GL_BLEND)
    glEnable(GL_DEPTH_TEST)
    glMatrixMode(GL_PROJECTION)
    glPopMatrix()
    glMatrixMode(GL_MODELVIEW)
    glPopMatrix()


# ── Quaternion math ──

def quat_normalize(q):
    w, x, y, z = q
    mag = math.sqrt(w*w + x*x + y*y + z*z)
    if mag < 1e-12:
        return (1.0, 0.0, 0.0, 0.0)
    return (w/mag, x/mag, y/mag, z/mag)


def quat_dot(a, b):
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3]


def quat_slerp(a, b, t):
    d = quat_dot(a, b)
    if d < 0.0:
        b = (-b[0], -b[1], -b[2], -b[3])
        d = -d
    if d > 0.9995:
        result = (
            a[0] + t*(b[0]-a[0]), a[1] + t*(b[1]-a[1]),
            a[2] + t*(b[2]-a[2]), a[3] + t*(b[3]-a[3]),
        )
        return quat_normalize(result)
    theta = math.acos(max(-1.0, min(1.0, d)))
    sin_theta = math.sin(theta)
    sa = math.sin((1.0-t)*theta) / sin_theta
    sb = math.sin(t*theta) / sin_theta
    return quat_normalize((
        sa*a[0]+sb*b[0], sa*a[1]+sb*b[1],
        sa*a[2]+sb*b[2], sa*a[3]+sb*b[3],
    ))


def quat_multiply(a, b):
    aw, ax, ay, az = a
    bw, bx, by, bz = b
    return (
        aw*bw - ax*bx - ay*by - az*bz,
        aw*bx + ax*bw + ay*bz - az*by,
        aw*by - ax*bz + ay*bw + az*bx,
        aw*bz + ax*by - ay*bx + az*bw,
    )


def quat_from_axis_angle(axis, angle_rad):
    half = angle_rad * 0.5
    s = math.sin(half)
    return quat_normalize((math.cos(half), axis[0]*s, axis[1]*s, axis[2]*s))


def quat_to_matrix(q):
    w, x, y, z = q
    return [
        1-2*(y*y+z*z), 2*(x*y+w*z),   2*(x*z-w*y),   0.0,
        2*(x*y-w*z),   1-2*(x*x+z*z), 2*(y*z+w*x),   0.0,
        2*(x*z+w*y),   2*(y*z-w*x),   1-2*(x*x+y*y), 0.0,
        0.0,           0.0,           0.0,            1.0,
    ]


def quat_from_gravity(ax, ay, az):
    mag = math.sqrt(ax*ax + ay*ay + az*az)
    if mag < 1e-9:
        return (1.0, 0.0, 0.0, 0.0)
    gx, gy, gz = ax/mag, ay/mag, az/mag
    d = gz
    if d < -0.9999:
        return (0.0, 1.0, 0.0, 0.0)
    w = 1.0 + d
    x = -gy
    y = gx
    z = 0.0
    return quat_normalize((w, x, y, z))


# ── BLE reader ──

async def ble_reader(shared, address=None):
    """Background async task: scan, connect, subscribe to notifications."""
    while shared["running"]:
        try:
            # Scan for the device
            if address:
                print(f"Connecting to {address}...")
                device = await BleakScanner.find_device_by_address(address, timeout=5.0)
            else:
                print(f"Scanning for '{DEVICE_NAME}'...")
                device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=5.0)

            if device is None:
                print("Device not found, retrying...")
                await asyncio.sleep(2)
                continue

            print(f"Found {device.name} ({device.address})")

            async with BleakClient(device, timeout=10.0) as client:
                print(f"Connected to {device.name}")
                shared["connected"] = True

                # Discover services and find our characteristics
                imu_char = None
                baro_char = None
                for service in client.services:
                    for char in service.characteristics:
                        uuid_lower = char.uuid.lower()
                        if uuid_lower == IMU_DATA_UUID.lower():
                            imu_char = char
                        elif uuid_lower == BARO_DATA_UUID.lower():
                            baro_char = char

                if imu_char is None and baro_char is None:
                    # Try reading all characteristics to find ours by size
                    print("Characteristics by UUID not found, listing all services:")
                    for service in client.services:
                        print(f"  Service: {service.uuid}")
                        for char in service.characteristics:
                            print(f"    Char: {char.uuid}  props={char.properties}")

                def imu_callback(sender, data: bytearray):
                    """Parse 28 bytes = 7 little-endian floats."""
                    if len(data) >= 28:
                        vals = struct.unpack("<7f", data[:28])
                        shared["ax"] = vals[0]
                        shared["ay"] = vals[1]
                        shared["az"] = vals[2]
                        shared["gx"] = vals[3]
                        shared["gy"] = vals[4]
                        shared["gz"] = vals[5]
                        shared["imu_temp"] = vals[6]
                        shared["gyro_time"] = time.monotonic()

                def baro_callback(sender, data: bytearray):
                    """Parse 12 bytes = 3 little-endian floats."""
                    if len(data) >= 8:
                        vals = struct.unpack("<3f", data[:12]) if len(data) >= 12 else struct.unpack("<2f", data[:8])
                        shared["pressure"] = vals[0]
                        shared["bmp_temp"] = vals[1]

                # Subscribe to notifications
                if imu_char:
                    await client.start_notify(imu_char, imu_callback)
                    print(f"  Subscribed to IMU notifications ({imu_char.uuid})")
                if baro_char:
                    await client.start_notify(baro_char, baro_callback)
                    print(f"  Subscribed to BARO notifications ({baro_char.uuid})")

                if not imu_char and not baro_char:
                    print("No matching characteristics found!")

                # Also poll via read if notifications aren't flowing
                while shared["running"] and client.is_connected:
                    # If we have characteristics but no notify, poll them
                    if imu_char and "read" in imu_char.properties:
                        try:
                            data = await client.read_gatt_char(imu_char)
                            imu_callback(None, data)
                        except Exception:
                            pass
                    if baro_char and "read" in baro_char.properties:
                        try:
                            data = await client.read_gatt_char(baro_char)
                            baro_callback(None, data)
                        except Exception:
                            pass
                    await asyncio.sleep(0.05)

                shared["connected"] = False
                print("Disconnected")

        except Exception as e:
            shared["connected"] = False
            print(f"BLE error: {e}")
            await asyncio.sleep(2)


def ble_thread(shared, address=None):
    """Run the BLE async loop in a background thread."""
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    try:
        loop.run_until_complete(ble_reader(shared, address))
    finally:
        loop.close()


# ── Main ──

def main():
    address = None
    if len(sys.argv) >= 2:
        address = sys.argv[1]

    shared = {
        "ax": 0.0, "ay": 0.0, "az": 1.0,
        "gx": 0.0, "gy": 0.0, "gz": 0.0,
        "gyro_time": 0.0,
        "imu_temp": 0.0,
        "pressure": 0.0, "bmp_temp": 0.0,
        "running": True, "connected": False,
    }

    t = threading.Thread(target=ble_thread, args=(shared, address), daemon=True)
    t.start()

    pygame.init()
    display = (800, 600)
    screen = pygame.display.set_mode(display, DOUBLEBUF | OPENGL)
    pygame.display.set_caption("Cyberboard BLE Demo")

    import os
    font_path = os.path.join(os.path.dirname(__file__), "BlenderProBold.ttf")
    if os.path.exists(font_path):
        font = pygame.font.Font(font_path, 16)
    else:
        print(f"Warning: {font_path} not found, falling back to system font")
        font = pygame.font.SysFont("consolas", 16)

    hud_surface = pygame.Surface(display, pygame.SRCALPHA)

    gluPerspective(45, display[0] / display[1], 0.1, 50.0)
    glTranslatef(0.0, 0.0, -7)
    glEnable(GL_DEPTH_TEST)
    glClearColor(0x0F / 255.0, 0x0F / 255.0, 0x0F / 255.0, 1.0)

    clock = pygame.time.Clock()

    smooth_q = (1.0, 0.0, 0.0, 0.0)
    alpha = 0.15

    cam_yaw = 0.0
    cam_pitch = 0.0
    cam_dist = 7.0
    dragging = False
    last_mouse = (0, 0)

    while shared["running"]:
        for event in pygame.event.get():
            if event.type == QUIT:
                shared["running"] = False
            elif event.type == KEYDOWN and event.key == K_ESCAPE:
                shared["running"] = False
            elif event.type == MOUSEBUTTONDOWN and event.button == 1:
                dragging = True
                last_mouse = event.pos
            elif event.type == MOUSEBUTTONUP and event.button == 1:
                dragging = False
            elif event.type == MOUSEMOTION and dragging:
                dx = event.pos[0] - last_mouse[0]
                dy = event.pos[1] - last_mouse[1]
                cam_yaw += dx * 0.3
                cam_pitch += dy * 0.3
                cam_pitch = max(-89.0, min(89.0, cam_pitch))
                last_mouse = event.pos
            elif event.type == MOUSEWHEEL:
                cam_dist -= event.y * 0.5
                cam_dist = max(2.0, min(20.0, cam_dist))

        ax, ay, az = shared["ax"], shared["ay"], shared["az"]
        gx, gy, gz = shared["gx"], shared["gy"], shared["gz"]
        gyro_time = shared["gyro_time"]

        last_gyro_time = gyro_time

        # Yaw (rotation around gravity) is disabled — without a magnetometer
        # the gyro drifts on this axis with no way to correct it.
        # Only tilt (pitch/roll from gravity) is used.
        target_q = quat_from_gravity(ax, ay, az)

        smooth_q = quat_slerp(smooth_q, target_q, alpha)
        mat = quat_to_matrix(smooth_q)

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        glLoadIdentity()
        gluPerspective(45, display[0] / display[1], 0.1, 50.0)
        glTranslatef(0.0, 0.0, -cam_dist)
        glRotatef(cam_pitch, 1, 0, 0)
        glRotatef(cam_yaw, 0, 1, 0)

        gl_mat = (ctypes.c_float * 16)(*mat)
        glMultMatrixf(gl_mat)

        draw_cube()
        draw_accel_arrows(ax, ay, az)
        draw_text_hud(hud_surface, font, shared)

        pygame.display.flip()
        clock.tick(60)

    pygame.quit()


if __name__ == "__main__":
    main()
