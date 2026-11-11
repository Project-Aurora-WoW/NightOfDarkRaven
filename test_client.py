import socket, struct, sys

def send_recv(host, port, opcode, body):
    s = socket.create_connection((host, port), timeout=5)
    s.settimeout(5)
    frame = struct.pack('<HH', len(body), opcode) + body
    s.sendall(frame)
    data = b''
    while len(data) < 4:
        chunk = s.recv(4 - len(data))
        if not chunk:
            raise EOFError('connection closed')
        data += chunk
    size, op_r = struct.unpack('<HH', data)
    data = b''
    while len(data) < size:
        chunk = s.recv(size - len(data))
        if not chunk:
            raise EOFError('connection closed')
        data += chunk
    s.close()
    return op_r, data

def read_string(data, pos):
    end = data.find(b'\x00', pos)
    return data[pos:end].decode('utf-8', 'replace'), end + 1

def main():
    # 1) Auth
    print('--- Auth test ---')
    body = b'raven\x00raven\x00'
    op, resp = send_recv('127.0.0.1', 3724, 0x0001, body)
    pos = 0
    result = resp[pos]; pos += 1
    print('auth result byte:', result)
    if result == 0:
        token = struct.unpack('<I', resp[pos:pos+4])[0]; pos += 4
        realm, pos = read_string(resp, pos)
        host, pos = read_string(resp, pos)
        port = struct.unpack('<H', resp[pos:pos+2])[0]; pos += 2
        print('token:', token)
        print('realm:', realm)
        print('host:', host)
        print('port:', port)
        global AUTH_TOKEN
        AUTH_TOKEN = token
        print('AUTH_OK -> world auth token', token)
    else:
        print('AUTH_FAILED')

    # 2) World auth
    print('--- World auth test ---')
    if 'AUTH_TOKEN' in globals():
        body = struct.pack('<I', AUTH_TOKEN)
        op, resp = send_recv('127.0.0.1', 8085, 0x0100, body)
        print('world auth result:', resp[0])

    # 3) Player login
    print('--- Player login test ---')
    body = b'RavenTest\x00'
    op, resp = send_recv('127.0.0.1', 8085, 0x0104, body)
    map_id = struct.unpack('<I', resp[:4])[0]
    x, y, z, o = struct.unpack('<ffff', resp[4:20])
    print('login verify: map=%d x=%.2f y=%.2f z=%.2f o=%.2f' % (map_id, x, y, z, o))

if __name__ == '__main__':
    main()
