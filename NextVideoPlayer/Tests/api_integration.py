"""Windows integration test. All player windows live on an inactive test desktop.
No third-party Python package, visible player window, or browser is required.
"""
import argparse
import base64
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import time
import urllib.request
import uuid

ROOT = Path(__file__).resolve().parents[1]
U, K = C.WinDLL('user32', use_last_error=True), C.WinDLL('kernel32', use_last_error=True)
U.CreateDesktopW.argtypes = [W.LPCWSTR, W.LPCWSTR, C.c_void_p, W.DWORD, W.DWORD, C.c_void_p]
U.CreateDesktopW.restype = W.HANDLE
U.CloseDesktop.argtypes = [W.HANDLE]
U.PostMessageW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM]
U.SendMessageW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM]
U.SendMessageW.restype = W.LPARAM
U.GetWindowThreadProcessId.argtypes = [W.HWND, C.POINTER(W.DWORD)]
U.GetClassNameW.argtypes = [W.HWND, W.LPWSTR, C.c_int]
U.GetWindowTextW.argtypes = [W.HWND, W.LPWSTR, C.c_int]
U.GetDlgItem.argtypes = [W.HWND, C.c_int]
U.GetDlgItem.restype = W.HWND
U.SetWindowTextW.argtypes = [W.HWND, W.LPCWSTR]
U.IsWindowVisible.argtypes = [W.HWND]
U.GetWindowLongW.argtypes = [W.HWND, C.c_int]
CALLBACK = C.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
U.EnumDesktopWindows.argtypes = [W.HANDLE, CALLBACK, W.LPARAM]
K.CloseHandle.argtypes = [W.HANDLE]
K.WaitForSingleObject.argtypes = [W.HANDLE, W.DWORD]
K.TerminateProcess.argtypes = [W.HANDLE, W.UINT]
K.GetProcessHandleCount.argtypes = [W.HANDLE, C.POINTER(W.DWORD)]
P = C.WinDLL('psapi', use_last_error=True)

class MemoryCounters(C.Structure):
    _fields_ = [('cb',W.DWORD), ('page_faults',W.DWORD)] + [
        (name,C.c_size_t) for name in ('peak_working_set','working_set','peak_paged_pool',
            'paged_pool','peak_nonpaged_pool','nonpaged_pool','pagefile','peak_pagefile','private')]
P.GetProcessMemoryInfo.argtypes = [W.HANDLE, C.c_void_p, W.DWORD]

class Startup(C.Structure):
    _fields_ = [('cb',W.DWORD),('reserved',W.LPWSTR),('desktop',W.LPWSTR),('title',W.LPWSTR),
        ('x',W.DWORD),('y',W.DWORD),('width',W.DWORD),('height',W.DWORD),('xc',W.DWORD),('yc',W.DWORD),
        ('fill',W.DWORD),('flags',W.DWORD),('show',W.WORD),('reserved2',W.WORD),('data',C.c_void_p),
        ('stdin',W.HANDLE),('stdout',W.HANDLE),('stderr',W.HANDLE)]
class ProcessInfo(C.Structure):
    _fields_=[('process',W.HANDLE),('thread',W.HANDLE),('pid',W.DWORD),('tid',W.DWORD)]
K.CreateProcessW.argtypes=[W.LPCWSTR,W.LPWSTR,C.c_void_p,C.c_void_p,W.BOOL,W.DWORD,C.c_void_p,W.LPCWSTR,C.POINTER(Startup),C.POINTER(ProcessInfo)]
K.CreateProcessW.restype=W.BOOL

def wait_for(check, timeout=10):
    end=time.monotonic()+timeout
    last=None
    while time.monotonic()<end:
        try:
            value=check()
            if value: return value
        except (OSError, ValueError, KeyError) as error: last=error
        time.sleep(.05)
    raise AssertionError(f'timed out: {last}')

class Player:
    def __init__(self, exe, config, port, desktop, enabled=True, files=(), ip='127.0.0.1'):
        config.write_text(f'[Playback]\nRepeat=1\n[API]\nEnabled={int(enabled)}\nIP={ip}\nPort={port}\n',encoding='utf-8')
        self.desktop=desktop
        startup=Startup(); startup.cb=C.sizeof(startup); startup.desktop=desktop[0]
        startup.flags=1; startup.show=1  # Visible only on the inactive test desktop.
        self.info=ProcessInfo()
        command=C.create_unicode_buffer(subprocess.list2cmdline([str(exe),'--config',str(config),*map(str,files)]))
        if not K.CreateProcessW(str(exe),command,None,None,False,0,None,str(ROOT),C.byref(startup),C.byref(self.info)):
            raise C.WinError(C.get_last_error())
        K.CloseHandle(self.info.thread)
    def alive(self): return K.WaitForSingleObject(self.info.process,0)==258
    def resources(self):
        memory=MemoryCounters(); memory.cb=C.sizeof(memory); handles=W.DWORD()
        assert P.GetProcessMemoryInfo(self.info.process,C.byref(memory),C.sizeof(memory))
        assert K.GetProcessHandleCount(self.info.process,C.byref(handles))
        return memory.private,handles.value
    def windows(self):
        found=[]
        @CALLBACK
        def visit(window,_):
            pid=W.DWORD(); U.GetWindowThreadProcessId(window,C.byref(pid))
            if pid.value==self.info.pid:
                cls=C.create_unicode_buffer(256); title=C.create_unicode_buffer(1024)
                U.GetClassNameW(window,cls,256); U.GetWindowTextW(window,title,1024)
                found.append((window,cls.value,title.value))
            return True
        U.EnumDesktopWindows(self.desktop[1],visit,0)
        return found
    def main(self): return next((item for item in self.windows() if item[1]=='NEXTVIDEOPLAYER'),None)
    def close(self):
        if self.alive():
            main=self.main()
            if main: U.PostMessageW(main[0],0x10,0,0)
            if K.WaitForSingleObject(self.info.process,5000)==258: K.TerminateProcess(self.info.process,1)
        K.CloseHandle(self.info.process)

class WebSocket:
    def __init__(self,port,origin=None,host=None,address='127.0.0.1'):
        self.socket=socket.create_connection((address,port),timeout=3)
        self.buffer=b''; self.serial=0
        key=base64.b64encode(os.urandom(16)).decode()
        headers=f'GET /api/v1 HTTP/1.1\r\nHost: {host or f"127.0.0.1:{port}"}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: {key}\r\n'
        if origin is not None: headers+=f'Origin: {origin}\r\n'
        self.socket.sendall((headers+'\r\n').encode())
        while b'\r\n\r\n' not in self.buffer:
            data=self.socket.recv(4096)
            if not data: raise ConnectionError('handshake rejected')
            self.buffer+=data
            if len(self.buffer)>16384: raise ConnectionError('header too large')
        response,self.buffer=self.buffer.split(b'\r\n\r\n',1)
        expected=base64.b64encode(hashlib.sha1((key+'258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest())
        if b' 101 ' not in response.split(b'\r\n')[0] or expected not in response:
            self.socket.close(); raise ConnectionError(response.decode(errors='replace'))
    def exact(self,count):
        while len(self.buffer)<count:
            data=self.socket.recv(max(4096,count-len(self.buffer)))
            if not data: raise ConnectionError('socket closed')
            self.buffer+=data
        result,self.buffer=self.buffer[:count],self.buffer[count:]
        return result
    def send_frame(self,data,opcode=1,final=True):
        mask=os.urandom(4); length=len(data)
        header=bytes([(0x80 if final else 0)|opcode])
        header+=bytes([0x80|length]) if length<126 else (b'\xfe'+struct.pack('!H',length) if length<65536 else b'\xff'+struct.pack('!Q',length))
        self.socket.sendall(header+mask+bytes(value^mask[i%4] for i,value in enumerate(data)))
    def receive(self):
        message=b''
        while True:
            first,second=self.exact(2); length=second&127
            if length==126: length=struct.unpack('!H',self.exact(2))[0]
            elif length==127: length=struct.unpack('!Q',self.exact(8))[0]
            assert not second&128, 'server frame must not be masked'
            payload=self.exact(length); opcode=first&15
            if opcode==8: raise ConnectionError('close frame')
            if opcode==9: self.send_frame(payload,10); continue
            if opcode==10: continue
            message+=payload
            if first&128: return json.loads(message.decode())
    def reply(self,request_id,timeout=4):
        end=time.monotonic()+timeout
        while time.monotonic()<end:
            result=self.receive()
            if result.get('requestId')==request_id: return result
        raise AssertionError('reply timed out')
    def request(self,values,expect=True):
        self.serial+=1; identifier=f'test-{self.serial}'
        self.send_frame(json.dumps({'type':'all','requestId':identifier,**values},ensure_ascii=False).encode())
        result=self.reply(identifier)
        assert result['ok']==expect, result
        return result
    def close(self):
        try: self.send_frame(struct.pack('!H',1000),8)
        except OSError: pass
        self.socket.close()

def free_port():
    with socket.socket() as sock: sock.bind(('127.0.0.1',0)); return sock.getsockname()[1]
def avi(path):
    def chunk(kind,data): return kind+struct.pack('<I',len(data))+data+(b'\0' if len(data)%2 else b'')
    def listing(kind,data): return chunk(b'LIST',kind+data)
    width=height=64; count=80; size=width*height*3
    header=struct.pack('<14I',100000,size*10,0,0x10,count,0,1,size,width,height,0,0,0,0)
    stream=struct.pack('<4s4sIHHIIIIIIIIhhhh',b'vids',b'DIB ',0,0,0,0,1,10,0,count,size,0xffffffff,0,0,0,width,height)
    bitmap=struct.pack('<IiiHHIIiiII',40,width,height,1,24,0,size,0,0,0,0)
    frames=b''; index=b''
    for number in range(count):
        data=bytes([number*3%256,90,180])*(width*height)
        index+=struct.pack('<4sIII',b'00db',0x10,4+len(frames),len(data)); frames+=chunk(b'00db',data)
    body=b'AVI '+listing(b'hdrl',chunk(b'avih',header)+listing(b'strl',chunk(b'strh',stream)+chunk(b'strf',bitmap)))+listing(b'movi',frames)+chunk(b'idx1',index)
    path.write_bytes(b'RIFF'+struct.pack('<I',len(body))+body)

def click(window,x,y):
    point=(y<<16)|x
    U.SendMessageW(window,0x201,1,point); U.SendMessageW(window,0x202,0,point)

def control_text(control):
    length=U.SendMessageW(control,0x0e,0,0)
    text=C.create_unicode_buffer(length+1)
    U.SendMessageW(control,0x0d,length+1,C.addressof(text))
    return text.value

def run(exe,stress_seconds=0):
    directory=ROOT/'artifacts'/'api-tests'/uuid.uuid4().hex; directory.mkdir(parents=True)
    desktop_name='NvpApi'+uuid.uuid4().hex
    desktop_handle=U.CreateDesktopW(desktop_name,None,None,0,0x1ff,None)
    if not desktop_handle: raise C.WinError(C.get_last_error())
    desktop=(desktop_name,desktop_handle); players=[]; ws=None
    try:
        port=free_port(); config=directory/'player.ini'
        player=Player(exe,config,port,desktop); players.append(player)
        def page():
            assert player.alive(), f'player exited: {player.windows()}'
            with urllib.request.urlopen(f'http://127.0.0.1:{port}/',timeout=.5) as response: return response.read()
        content=wait_for(page,20)
        assert 'NextVideoPlayer 원격 제어'.encode() in content
        ws=WebSocket(port)
        initial=ws.request({'subscribe':False})['state']; assert initial['protocolVersion']==1
        changed=ws.request({'volume':0,'muted':False,'speed':.20,'repeat':False})['state']
        assert changed['volume']==0 and changed['muted'] is False and changed['speed']==.20
        kept=ws.request({'volume':None,'muted':'','speed':'','repeat':None})['state']
        assert kept['volume']==0 and kept['muted'] is False and kept['speed']==.20
        ws.request({'volume':80,'speed':17},False)
        assert ws.request({})['state']['volume']==0
        ws.send_frame(b'{"type":"all","requestId":"fragment",',final=False)
        ws.send_frame(b'"volume":33}',opcode=0)
        assert ws.reply('fragment')['state']['volume']==33
        for raw in (b'{"type":"all","requestId":"bad","volume":1,"volume":2}',b'{"type":"all","requestId":"bad","positionMs":-1}',b'{"type":"all","requestId":"bad","unknown":true}'):
            ws.send_frame(raw); assert ws.reply('bad')['ok'] is False
        ws.send_frame(b'{"type":"all","volume":999,"requestId":"id-last"}')
        assert ws.reply('id-last')['ok'] is False
        ws.send_frame(json.dumps({'type':'all','requestId':r'literal\u0000'}).encode())
        assert ws.reply(r'literal\u0000')['ok'] is True
        ws.send_frame(b'{"type":"all","requestId":"deep","volume":'+b'['*33+b'0'+b']'*33+b'}')
        assert ws.receive()['ok'] is False
        print('PASS: live HTTP control page, WebSocket framing, partial updates, zero/false/null/empty, validation, fragmented input.')
        for origin,host in [('https://evil.example',None),('https://'+'a'*600,None),(None,f'evil.example:{port}')]:
            try: bad=WebSocket(port,origin,host); bad.close(); raise AssertionError('untrusted origin/host accepted')
            except (ConnectionError,OSError): pass
        print('PASS: cross-origin browser control and DNS-rebinding Host headers rejected.')
        browser=WebSocket(port,origin=f'http://127.0.0.1:{port}')
        assert browser.request({'subscribe':False})['ok']; browser.close()
        for payload,opcode in [(b'binary',2),(b'x'*(1024*1024+1),1)]:
            invalid=WebSocket(port)
            try:
                invalid.send_frame(payload,opcode)
                while True: invalid.receive()
            except (ConnectionError,OSError): pass
            finally: invalid.socket.close()
        peers=[]
        try:
            for _ in range(7):
                peer=WebSocket(port); peer.request({'subscribe':False}); peers.append(peer)
            try:
                extra=WebSocket(port); extra.close(); raise AssertionError('connection limit exceeded')
            except (ConnectionError,OSError): pass
        finally:
            for peer in peers: peer.close()
        print('PASS: binary/oversized messages and excess clients rejected; same-origin browser accepted.')
        sample1=directory/'sample one.avi'; sample2=directory/'sample two.avi'; avi(sample1); avi(sample2)
        loaded=ws.request({'files':[str(sample1),str(sample2)],'playback':'paused','positionMs':1200})
        assert loaded['status']=='accepted'
        def ready():
            state=ws.request({})['state']
            if state['mediaError']: raise AssertionError(state['mediaError'])
            return state if state['ready'] and not state['loading'] and abs(state['positionMs']-1200)<400 else None
        state=wait_for(ready,12); assert state['playlistCount']==2 and state['playback']=='paused'
        sample_response=subprocess.run(['pwsh','-NoProfile','-File',str(ROOT/'API'/'Send-PlayerCommand.ps1'),
            '-Endpoint',f'ws://127.0.0.1:{port}/api/v1'],capture_output=True,timeout=10,encoding='utf-8')
        assert sample_response.returncode==0, sample_response.stderr
        try: assert json.loads(sample_response.stdout)['ok'] is True
        except ValueError as error: raise AssertionError(repr(sample_response.stdout)) from error
        ws.request({'speed':1,'playback':'playing'})
        last={}
        def advanced():
            nonlocal last
            last=ws.request({})['state']
            return last if last['positionMs']>state['positionMs']+100 else None
        try: wait_for(advanced,4)
        except AssertionError as error: raise AssertionError(f'Playback did not advance: before={state}, after={last}') from error
        ws.request({'playback':'paused'})
        ws.request({'files':[str(directory/'missing.mp4')],'volume':95},False)
        target=ws.request({})['state']['timestampMs']+450
        assert ws.request({'executeAtMs':target,'volume':27})['status']=='accepted'
        time.sleep(.6); assert ws.request({})['state']['volume']==27
        print('PASS: actual libmpv file load, multi-file playlist, ready/paused/seek/play states and scheduled execution.')
        # Open the real settings window only on the inactive desktop. Apply a new
        # API port while a remote volume change is received during the modal loop.
        main=wait_for(player.main)
        U.PostMessageW(main[0],0x111,233,0)
        settings=wait_for(lambda:next((item for item in player.windows() if item[1]=='NextVideoPlayer_SettingsWnd'),None))
        click(settings[0],532,85)
        information=U.GetDlgItem(settings[0],701)
        assert information and U.IsWindowVisible(information), 'app information tab is hidden'
        about=control_text(information)
        for expected in ('SUNGJONG','tjdwhd70599@gmail.com','최소 사양','권장 사양','성능 검증 전','libwebsockets','cJSON'):
            assert expected in about, (expected,about)
        assert U.GetWindowLongW(information,-16)&0x800, 'app information must be read-only'
        click(settings[0],488,366)
        licenses=control_text(information)
        assert len(licenses)>10000 and 'GNU LESSER GENERAL PUBLIC LICENSE' in licenses
        assert 'Dave Gamble' in licenses and 'libwebsockets' in licenses.lower()
        assert 'OPEN FONT LICENSE' in licenses.upper(), 'font license missing'
        assert 'Version 3, 29 June 2007' in licenses and len(licenses)>60000, 'LGPLv3/GPLv3 license text truncated'
        click(settings[0],488,366)
        assert control_text(information)==about, 'return from license view changed app information'
        print('PASS: app information, contact details, provisional system requirements and embedded full license viewer.')
        click(settings[0],438,85)
        assert not U.IsWindowVisible(information), 'app information overlaps the API tab'
        new_port=free_port()
        edit=U.GetDlgItem(settings[0],603)
        assert edit, f'port editor not found: {player.windows()}'
        # Cross-process edit controls require WM_SETTEXT/WM_GETTEXT. The window
        # caption cache changed by SetWindowTextW is separate from edit contents.
        port_text=C.create_unicode_buffer(str(new_port))
        assert U.SendMessageW(edit,0x0c,0,C.addressof(port_text))
        edit_text=C.create_unicode_buffer(16); U.SendMessageW(edit,0x0d,16,C.addressof(edit_text))
        assert edit_text.value==str(new_port), (edit_text.value,new_port)
        ws.request({'volume':42})
        U.SendMessageW(edit,0x0d,16,C.addressof(edit_text))
        assert edit_text.value==str(new_port), ('after API update',edit_text.value,new_port)
        click(settings[0],510,434)
        ws.close(); ws=None
        try: wait_for(lambda:urllib.request.urlopen(f'http://127.0.0.1:{new_port}/',timeout=.5).read())
        except AssertionError as error:
            raise AssertionError(f'Port change failed: config={config.read_text(encoding="utf-8")}, windows={player.windows()}, old={port}, new={new_port}, alive={player.alive()}') from error
        ws=WebSocket(new_port); assert ws.request({'subscribe':False})['state']['volume']==42
        assert f'Port={new_port}' in config.read_text(encoding='utf-8')
        print('PASS: real settings API tab, port change/restart, INI persistence and modal-dialog state merge.')
        # A second player cannot bind the same port but must still play normally.
        second=Player(exe,directory/'conflict.ini',new_port,desktop,files=[sample1]); players.append(second)
        wait_for(lambda: second.alive() and second.main() and 'sample one.avi' in second.main()[2] and '재생 중' in second.main()[2],15)
        assert ws.request({})['ok']
        disabled=Player(exe,directory/'disabled.ini',free_port(),desktop,enabled=False,files=[sample2]); players.append(disabled)
        wait_for(lambda: disabled.alive() and disabled.main() and 'sample two.avi' in disabled.main()[2] and '재생 중' in disabled.main()[2],15)
        print('PASS: occupied API port and disabled API do not prevent ordinary video playback.')
        for _ in range(50):
            client=WebSocket(new_port); client.request({'subscribe':False}); client.close()
        for _ in range(100): ws.request({})
        print('PASS: repeated reconnects and requests. All player windows stayed on the inactive desktop.')
        if stress_seconds:
            ws.request({'repeat':True,'speed':4,'playback':'playing','positionMs':0})
            time.sleep(2)
            baseline=player.resources(); end=time.monotonic()+stress_seconds
            maximum=list(baseline); previous_file=None; transitions=0; requests=0
            while time.monotonic()<end:
                state=ws.request({})['state']; requests+=1
                if previous_file and previous_file!=state['file']: transitions+=1
                previous_file=state['file']
                assert not state['mediaError'], state
                if requests%5==0:
                    peer=WebSocket(new_port); peer.request({'subscribe':False}); peer.close()
                resources=player.resources()
                maximum=[max(a,b) for a,b in zip(maximum,resources)]
                time.sleep(.03)
            final=player.resources()
            assert transitions>=2, f'Playlist did not loop: {transitions}'
            assert final[0]-baseline[0]<48*1024*1024, (baseline,final)
            assert final[1]-baseline[1]<32, (baseline,final)
            print(f'PASS: {stress_seconds}s repeat-playback stress, {requests} queries, {transitions} file transitions; '
                f'private bytes/handles baseline={baseline}, final={final}, peak={maximum}.')
    finally:
        if ws: ws.close()
        for player in reversed(players): player.close()
        U.CloseDesktop(desktop_handle)
    print('Logs and test data:',directory)

if __name__=='__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('--exe',type=Path,default=ROOT/'NextVideoPlayer.exe')
    parser.add_argument('--stress-seconds',type=int,default=0)
    options=parser.parse_args()
    run(options.exe.resolve(),options.stress_seconds)
