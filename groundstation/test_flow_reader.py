import asyncio
import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import check_flow_height as reader

class Socket:
    def __init__(self):
        self.sent=[]
        self.messages=[
            {'flag':3,'fields':{'armed':0}},
            {'flag':18,'fields':{'flow_height_source':2,'flow_height_filtered':.8,'flow_height_vz':-.03}},
            {'flag':1,'fields':{'height':.2,'vertical_velocity':.12}},
        ]
    async def __aenter__(self):return self
    async def __aexit__(self,*args):return False
    async def send(self,value):self.sent.append(json.loads(value))
    async def recv(self):
        if self.messages:return json.dumps(self.messages.pop(0))
        await asyncio.sleep(1)

class ReaderTests(unittest.IsolatedAsyncioTestCase):
    async def test_reader_only_authenticates_as_viewer_and_records_telemetry(self):
        socket=Socket()
        with tempfile.TemporaryDirectory() as temp:
            with patch.object(reader,'ROOT',Path(temp)),patch.object(reader.websockets,'connect',return_value=socket),contextlib.redirect_stdout(io.StringIO()):
                await reader.main(.05)
            self.assertEqual(socket.sent,[{'auth':{'role':'viewer'}}])
            files=list((Path(temp)/'logs'/'flow_height').glob('*.jsonl'))
            self.assertEqual(len(files),1)
            rows=[json.loads(x) for x in files[0].read_text().splitlines()]
            self.assertEqual([row['value']['flag'] for row in rows],[3,18,1])

if __name__=='__main__':unittest.main()
