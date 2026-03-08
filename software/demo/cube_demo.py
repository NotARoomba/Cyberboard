"""
3D cube demo driven by ICM-42688-PC accelerometer data over serial.

Reads lines like:
    A:0.01,-0.03,1.00 G:0.1,0.2,-0.3 T:25.0
    P:101325.00 Pa  T:25.00 C

Renders a cube oriented by the gravity vector (quaternion-based, no gimbal lock),
with acceleration arrows shooting from the cube faces, and a text HUD showing
temperature, pressure, and estimated altitude.

Usage:
    python cube_demo.py COM5          # Windows
    python cube_demo.py /dev/ttyACM0  # Linux
"""

import sys
import math
import time
import threading
import ctypes
import serial
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

# Sea-level pressure for altitude estimation (Pa)
SEA_LEVEL_PA = 101325.0


# ── Cube geometry ──

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
    """Draw a 3D arrow from origin along direction with given length."""
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

    # Arrowhead — small cone approximated by triangles
    head_len = min(0.3, length * 0.3)
    head_r = 0.08

    # Find two perpendicular vectors to direction
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

    # Base of cone
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
    """Draw arrows from cube faces representing acceleration on each axis."""
    scale = 2.0  # length per g

    # X axis — right face (+X)
    if abs(ax) > 0.01:
        sign = 1.0 if ax > 0 else -1.0
        draw_arrow(
            (sign * 1.0, 0, 0), (sign, 0, 0), abs(ax) * scale,
            (1.0, 0.4, 0.4)
        )

    # Y axis — top face (+Y)
    if abs(ay) > 0.01:
        sign = 1.0 if ay > 0 else -1.0
        draw_arrow(
            (0, sign * 1.0, 0), (0, sign, 0), abs(ay) * scale,
            (0.4, 1.0, 0.4)
        )

    # Z axis — front face (+Z)
    if abs(az) > 0.01:
        sign = 1.0 if az > 0 else -1.0
        draw_arrow(
            (0, 0, sign * 1.0), (0, 0, sign), abs(az) * scale,
            (0.4, 0.4, 1.0)
        )


# ── Text HUD overlay ──

def draw_text_hud(surface, font, shared):
    """Render text HUD onto a pygame surface, then blit to OpenGL."""
    w, h = surface.get_size()
    surface.fill((0, 0, 0, 0))

    lines = []

    # IMU data
    lines.append(f"Accel: {shared['ax']:+.2f}, {shared['ay']:+.2f}, {shared['az']:+.2f} g")
    lines.append(f"Gyro:  {shared['gx']:+.1f}, {shared['gy']:+.1f}, {shared['gz']:+.1f} dps")
    lines.append(f"IMU Temp: {shared['imu_temp']:.1f} C")

    lines.append("")

    # BMP580 data
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

    y = 10
    for line in lines:
        if line == "":
            y += 8
            continue
        text_surf = font.render(line, True, (220, 220, 220))
        surface.blit(text_surf, (10, y))
        y += text_surf.get_height() + 2

    # Connection status in bottom-left
    status = "CONNECTED" if shared["connected"] else "DISCONNECTED"
    color = (100, 255, 100) if shared["connected"] else (255, 100, 100)
    status_surf = font.render(status, True, color)
    surface.blit(status_surf, (10, h - status_surf.get_height() - 10))

    # Blit pygame surface to OpenGL
    text_data = pygame.image.tostring(surface, "RGBA", True)

    # Switch to 2D orthographic projection
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
    mag = math.sqrt(w * w + x * x + y * y + z * z)
    if mag < 1e-12:
        return (1.0, 0.0, 0.0, 0.0)
    return (w / mag, x / mag, y / mag, z / mag)


def quat_dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]


def quat_slerp(a, b, t):
    d = quat_dot(a, b)
    if d < 0.0:
        b = (-b[0], -b[1], -b[2], -b[3])
        d = -d
    if d > 0.9995:
        result = (
            a[0] + t * (b[0] - a[0]),
            a[1] + t * (b[1] - a[1]),
            a[2] + t * (b[2] - a[2]),
            a[3] + t * (b[3] - a[3]),
        )
        return quat_normalize(result)
    theta = math.acos(max(-1.0, min(1.0, d)))
    sin_theta = math.sin(theta)
    sa = math.sin((1.0 - t) * theta) / sin_theta
    sb = math.sin(t * theta) / sin_theta
    return quat_normalize((
        sa * a[0] + sb * b[0],
        sa * a[1] + sb * b[1],
        sa * a[2] + sb * b[2],
        sa * a[3] + sb * b[3],
    ))


def quat_multiply(a, b):
    """Hamilton product of two quaternions."""
    aw, ax, ay, az = a
    bw, bx, by, bz = b
    return (
        aw*bw - ax*bx - ay*by - az*bz,
        aw*bx + ax*bw + ay*bz - az*by,
        aw*by - ax*bz + ay*bw + az*bx,
        aw*bz + ax*by - ay*bx + az*bw,
    )


def quat_from_axis_angle(axis, angle_rad):
    """Create quaternion from axis (unit vector) and angle in radians."""
    half = angle_rad * 0.5
    s = math.sin(half)
    return quat_normalize((math.cos(half), axis[0]*s, axis[1]*s, axis[2]*s))


def quat_to_matrix(q):
    w, x, y, z = q
    return [
        1 - 2*(y*y + z*z),  2*(x*y + w*z),      2*(x*z - w*y),      0.0,
        2*(x*y - w*z),      1 - 2*(x*x + z*z),  2*(y*z + w*x),      0.0,
        2*(x*z + w*y),      2*(y*z - w*x),       1 - 2*(x*x + y*y), 0.0,
        0.0,                0.0,                 0.0,                1.0,
    ]


def quat_from_gravity(ax, ay, az):
    mag = math.sqrt(ax * ax + ay * ay + az * az)
    if mag < 1e-9:
        return (1.0, 0.0, 0.0, 0.0)
    gx, gy, gz = ax / mag, ay / mag, az / mag
    d = gz
    if d < -0.9999:
        return (0.0, 1.0, 0.0, 0.0)
    w = 1.0 + d
    x = -gy
    y = gx
    z = 0.0
    return quat_normalize((w, x, y, z))


# ── Serial ──

def serial_reader(port, baud, shared):
    """Background thread: read serial, parse IMU + BMP580, auto-reconnect."""
    while shared["running"]:
        ser = None
        try:
            ser = serial.Serial(port, baud, timeout=1)
            shared["connected"] = True
            print(f"Connected to {port}")
        except serial.SerialException:
            shared["connected"] = False
            time.sleep(1)
            continue

        while shared["running"]:
            try:
                line = ser.readline().decode("utf-8", errors="replace").strip()
            except (serial.SerialException, OSError):
                print(f"Lost connection to {port}, reconnecting...")
                shared["connected"] = False
                break

            # IMU line: A:ax,ay,az G:gx,gy,gz T:temp
            if line.startswith("A:"):
                try:
                    parts = line.split()
                    accel_str = parts[0][2:]
                    ax, ay, az = (float(v) for v in accel_str.split(","))
                    shared["ax"] = ax
                    shared["ay"] = ay
                    shared["az"] = az

                    if len(parts) >= 2 and parts[1].startswith("G:"):
                        gyro_str = parts[1][2:]
                        gx, gy, gz = (float(v) for v in gyro_str.split(","))
                        shared["gx"] = gx
                        shared["gy"] = gy
                        shared["gz"] = gz
                        shared["gyro_time"] = time.monotonic()

                    if len(parts) >= 3 and parts[2].startswith("T:"):
                        shared["imu_temp"] = float(parts[2][2:])
                except (ValueError, IndexError):
                    pass

            # BMP580 line: P:101325.00 Pa  T:25.00 C
            elif line.startswith("P:"):
                try:
                    parts = line.split()
                    shared["pressure"] = float(parts[0][2:])
                    # Find T: in the line
                    for i, p in enumerate(parts):
                        if p.startswith("T:"):
                            shared["bmp_temp"] = float(p[2:])
                            break
                except (ValueError, IndexError):
                    pass

        if ser is not None:
            try:
                ser.close()
            except Exception:
                pass
        time.sleep(1)


# ── Main ──

def main():
    if len(sys.argv) < 2:
        print("Usage: python cube_demo.py <serial_port> [baud]")
        print("  e.g. python cube_demo.py COM5")
        sys.exit(1)

    port = sys.argv[1]
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else 115200

    shared = {
        "ax": 0.0, "ay": 0.0, "az": 1.0,
        "gx": 0.0, "gy": 0.0, "gz": 0.0,
        "gyro_time": 0.0,
        "imu_temp": 0.0,
        "pressure": 0.0, "bmp_temp": 0.0,
        "running": True, "connected": False,
    }

    t = threading.Thread(target=serial_reader, args=(port, baud, shared), daemon=True)
    t.start()

    pygame.init()
    display = (800, 600)
    screen = pygame.display.set_mode(display, DOUBLEBUF | OPENGL)
    pygame.display.set_caption("Cyberboard Demo")

    import os
    font_path = os.path.join(os.path.dirname(__file__), "BlenderProBold.ttf")
    if os.path.exists(font_path):
        font = pygame.font.Font(font_path, 16)
    else:
        print(f"Warning: {font_path} not found, falling back to system font")
        font = pygame.font.SysFont("consolas", 16)

    # Transparent surface for text overlay
    hud_surface = pygame.Surface(display, pygame.SRCALPHA)

    gluPerspective(45, display[0] / display[1], 0.1, 50.0)
    glTranslatef(0.0, 0.0, -7)
    glEnable(GL_DEPTH_TEST)
    glClearColor(0x0F / 255.0, 0x0F / 255.0, 0x0F / 255.0, 1.0)

    clock = pygame.time.Clock()

    smooth_q = (1.0, 0.0, 0.0, 0.0)
    yaw_angle = 0.0  # integrated gyro yaw in radians
    last_gyro_time = 0.0
    alpha = 0.15

    # Camera orbit state (left-click drag to rotate, scroll to zoom)
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

        # Integrate gyro yaw (rotation around gravity axis)
        if last_gyro_time > 0 and gyro_time > last_gyro_time:
            dt = gyro_time - last_gyro_time
            dt = min(dt, 0.1)  # clamp to avoid jumps on reconnect

            # Project gyro onto gravity direction to get yaw rate
            # Gravity vector (normalized)
            g_mag = math.sqrt(ax*ax + ay*ay + az*az)
            if g_mag > 0.1:
                gnx, gny, gnz = ax/g_mag, ay/g_mag, az/g_mag
                # Gyro component along gravity = dot(gyro, gravity_unit)
                yaw_rate_dps = gx*gnx + gy*gny + gz*gnz
                yaw_angle += math.radians(yaw_rate_dps) * dt
        last_gyro_time = gyro_time

        # Accel quaternion gives pitch/roll (tilt relative to gravity)
        tilt_q = quat_from_gravity(ax, ay, az)

        # Yaw quaternion rotates around the gravity axis
        # Gravity direction in body frame (normalized)
        g_mag = math.sqrt(ax*ax + ay*ay + az*az)
        if g_mag > 0.1:
            gn = (ax/g_mag, ay/g_mag, az/g_mag)
        else:
            gn = (0, 0, 1)
        yaw_q = quat_from_axis_angle(gn, yaw_angle)

        # Combined: first tilt, then yaw around gravity
        target_q = quat_multiply(yaw_q, tilt_q)

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

        # Text HUD
        draw_text_hud(hud_surface, font, shared)

        pygame.display.flip()
        clock.tick(60)

    pygame.quit()


if __name__ == "__main__":
    main()
