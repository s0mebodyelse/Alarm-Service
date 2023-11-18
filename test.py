#!/usr/bin/python3

import asyncio
import random
import time
import struct


async def test_client(clients: int):
    tasks = []
    for i in range(clients):
        wait_secs = random.randrange(10) + 1
        tasks.append(asyncio.ensure_future(set_timer("Wake up", wait_secs, i)))
    return await asyncio.gather(*tasks)

async def set_timer(message, wait_secs, id):
    reader, writer = await asyncio.open_connection("127.0.0.1", 8088)

    # 5 secs from now
    due_time = int(time.time() + wait_secs)

    header = struct.pack("! I Q I", id, due_time, len(message))

    writer.write(header)
    await writer.drain()

    print(f'Send message: {message}') 
    writer.write(message.encode())
    await writer.drain()

    # read the header
    request_id = await reader.readexactly(4)
    print(f'Received request_id: {request_id}')

    cookie_size = await reader.readexactly(4)
    print(f'Received cookie_size: {int.from_bytes(cookie_size, "little")}')
    
    # read the data
    data = await reader.read(int.from_bytes(cookie_size, 'bit'))
    #print(f'Received data: {data}')

    return data

def main():
    loop = asyncio.get_event_loop()
    future = asyncio.ensure_future(test_client(10))
    return loop.run_until_complete(future)

if __name__ == "__main__":
    res = main()
    print(res)

    for i in res:
        print(i)
