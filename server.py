# -*- coding: utf-8 -*-
# @Author: amaneureka
# @Date:   2017-04-01 16:07:30
# @Last Modified by:   amaneureka
# @Last Modified time: 2017-04-01 22:26:04

import sys
import uuid
import socket
import select
import logging
import hashlib
import configparser
import sqlite3 as sql

from enum import Enum
class REQUEST(Enum):

    # S2C --> Server to Client
    # C2S --> Client to Server

    IDENTIFY    = 'IDY' # S2C : Prove your identity
    REGISTER    = 'REG' # C2S : Register me as a new device
    INVALID     = 'INV' # C2S : Invalid Request
    LOGIN       = 'LOG' # C2S : Login me with my UID
    UID         = 'UID' # S2C : Here is your new UID
    HELLO       = 'HLO' # S2C : Hello! login successful
    COMMAND     = 'CMD' # S2C : Execute Command
    RESPONSE    = 'RSP' # C2S : Response of Command

def get_request_header(data):
    try:
        res = REQUEST(data[:3])
    except:
        res = REQUEST.INVALID
    return res

def send_request(sock, request, data=''):
    logging.debug('[%s] \'%s\' sent', str(sock.getpeername()), request.name)
    sock.send(request.value + str(data))

def register_new_device(connection, key):
    cursor = connection.cursor()

    t = (key, )
    cursor.execute('INSERT INTO clients (key) VALUES (?)', t)
    connection.commit()

def get_device_id_from_key(connection, key):
    cursor = connection.cursor()

    t = (key, )
    row = cursor.execute('SELECT id FROM clients WHERE key=?', t)
    return row.fetchone()[0]

def setup_database(connection):
    cursor = connection.cursor()

    t = (str(uuid.uuid4()), )
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS clients
        (
            id INTEGER primary key NOT NULL,
            key VARCHAR
        );''');
    try:
        cursor.execute('INSERT INTO clients (id, key) VALUES (0, ?);', t)
    except:
        pass
    connection.commit()

def start_server():

    server_config = config['Server']

    # configurations
    HOST = server_config['HOST']
    PORT = server_config.getint('PORT')
    BUFFER_SIZE = server_config.getint('BUFFER_SIZE')

    # server socket setup
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind((HOST, PORT))
    server_socket.listen(server_config.getint('LIMIT'))

    # server database setup
    sql_connection = sql.connect(server_config['DATABASE'])
    setup_database(sql_connection)

    LABEL_2_ID = { }
    ID_2_SOCKET = { }
    SOCKET_LIST = []

    SOCKET_LIST.append(server_socket)

    logging.info('server started %s:%d', HOST, PORT)

    while True:

        # list of sockets which are ready to read
        ready_to_read, _, _ = select.select(SOCKET_LIST, [], [], 0)

        for sock in ready_to_read:

            # new connection
            if sock == server_socket:

                sockfd, addr = server_socket.accept()

                logging.info('[%s:%s] connected' % addr)

                # send 'IDENTIFY' request
                try:

                    send_request(sockfd, REQUEST.IDENTIFY)
                    SOCKET_LIST.append(sockfd)

                except:

                    logging.info('[%s:%s] disconnected')

            # message from client
            else:

                # process data
                try:

                    data = sock.recv(BUFFER_SIZE)
                    header = get_request_header(data)
                    label = str(sock.getpeername())

                    logging.debug('[%s] requested \'%s\'', label, header.name)

                    if header == REQUEST.REGISTER:

                        key = str(uuid.uuid4())
                        register_new_device(sql_connection, key)
                        send_request(sock, REQUEST.UID, key)

                    elif header == REQUEST.LOGIN:

                        key = data[3:].strip()
                        logging.debug('\tkey \'%s\'', key)

                        device_id = get_device_id_from_key(sql_connection, key)
                        logging.debug('\tdevice id \'%s\'', device_id)

                        LABEL_2_ID[label] = device_id
                        ID_2_SOCKET[device_id] = sock
                        send_request(sock, REQUEST.HELLO)

                    elif header == REQUEST.RESPONSE:

                        if label not in LABEL_2_ID:
                            continue

                        try:
                            ID_2_SOCKET[0].send(data[3:])
                        except:
                            if 0 in ID_2_SOCKET[0]:
                                ID_2_SOCKET[0].close()
                                ID_2_SOCKET.pop(0, None)

                    elif header == REQUEST.COMMAND:

                        if label not in LABEL_2_ID or LABEL_2_ID[label] != 0:
                            continue

                        request_id = int(data[3:7])
                        cmd = data[7:]

                        if request_id in ID_2_SOCKET:
                            ID_2_SOCKET[request_id].send(cmd)

                    else:
                        if sock in SOCKET_LIST:
                            SOCKET_LIST.remove(sock)
                        raise ValueError('Invalid Header')

                except Exception as error:
                    logging.error(str(error))

    server_socket.close()

if __name__ == '__main__':
    config = configparser.ConfigParser()
    config.read('config.ini')
    logging.basicConfig(stream=sys.stderr, level=logging.DEBUG)
    sys.exit(start_server())
