import struct
import sys

def parse_minidump(filename):
    with open(filename, 'rb') as f:
        header = f.read(32)
        sig, ver, num_streams, stream_rva = struct.unpack('<4sIII', header[:16])
        if sig != b'MDMP':
            print("Not a valid minidump file")
            return
        
        streams = []
        f.seek(stream_rva)
        for _ in range(num_streams):
            st_type, data_sz, rva = struct.unpack('<III', f.read(12))
            streams.append((st_type, data_sz, rva))
            
        def read_string(rva):
            f.seek(rva)
            length = struct.unpack('<I', f.read(4))[0]
            raw = f.read(length)
            return raw.decode('utf-16-le', errors='replace')

        modules = []
        exception = None
        threads = []

        for st_type, data_sz, rva in streams:
            if st_type == 3: # ThreadListStream
                f.seek(rva)
                num_threads = struct.unpack('<I', f.read(4))[0]
                for _ in range(num_threads):
                    tdata = f.read(48)
                    th_id, sus_cnt, prio_class, prio, teb = struct.unpack('<IIIIQ', tdata[:24])
                    stack_start = struct.unpack('<Q', tdata[24:32])[0]
                    stack_sz, stack_rva = struct.unpack('<II', tdata[32:40])
                    ctx_sz, ctx_rva = struct.unpack('<II', tdata[40:48])
                    threads.append({
                        'id': th_id,
                        'stack_start': stack_start,
                        'stack_sz': stack_sz,
                        'stack_rva': stack_rva,
                        'ctx_sz': ctx_sz,
                        'ctx_rva': ctx_rva
                    })
            elif st_type == 4: # ModuleListStream
                f.seek(rva)
                num_mods = struct.unpack('<I', f.read(4))[0]
                for _ in range(num_mods):
                    mod_data = f.read(108)
                    base, size, chk, stamp, name_rva = struct.unpack('<QIIII', mod_data[:24])
                    modules.append((base, size, name_rva))
            elif st_type == 6: # ExceptionStream
                f.seek(rva)
                th_id, _, code, flags, rec_ptr, addr, num_params = struct.unpack('<IIIIQQI', f.read(36))
                f.seek(rva + 40)
                params = struct.unpack('<15Q', f.read(120))
                ctx_sz, ctx_rva = struct.unpack('<II', f.read(8))
                exception = {
                    'th_id': th_id,
                    'code': code,
                    'flags': flags,
                    'addr': addr,
                    'num_params': num_params,
                    'params': params[:num_params],
                    'ctx_rva': ctx_rva,
                    'ctx_sz': ctx_sz
                }

        resolved_mods = []
        for base, size, name_rva in modules:
            name = read_string(name_rva)
            resolved_mods.append((base, size, name))

        print(f"Exception: 0x{exception['code']:08x} at 0x{exception['addr']:016x}, Thread 0x{exception['th_id']:x}")

        # Find crashing thread
        target_thread = None
        for t in threads:
            if t['id'] == exception['th_id']:
                target_thread = t
                break

        # Read context RSP
        f.seek(exception['ctx_rva'])
        ctx_data = f.read(exception['ctx_sz'])
        rsp = struct.unpack('<Q', ctx_data[0x98:0xa0])[0]
        rip = struct.unpack('<Q', ctx_data[0xf8:0x100])[0]
        print(f"Context RIP: 0x{rip:016x}, RSP: 0x{rsp:016x}")

        if target_thread:
            stack_start = target_thread['stack_start']
            stack_end = stack_start + target_thread['stack_sz']
            print(f"Stack range: 0x{stack_start:016x} - 0x{stack_end:016x}")
            
            f.seek(target_thread['stack_rva'])
            stack_bytes = f.read(target_thread['stack_sz'])
            
            # Start scan at RSP
            start_offset = 0
            if stack_start <= rsp < stack_end:
                start_offset = rsp - stack_start
            
            print(f"\n--- STACK TRACE FROM RSP (offset 0x{start_offset:x}) ---")
            found = 0
            for i in range(start_offset, len(stack_bytes) - 7, 8):
                val = struct.unpack('<Q', stack_bytes[i:i+8])[0]
                for base, size, name in resolved_mods:
                    if base <= val < base + size:
                        mod_name = name.split('\\')[-1]
                        offset = val - base
                        curr_sp = stack_start + i
                        print(f"  [0x{curr_sp:016x}] 0x{val:016x} -> {mod_name}+0x{offset:x}")
                        found += 1
                        if found > 60:
                            return

if __name__ == '__main__':
    dump_path = sys.argv[1] if len(sys.argv) > 1 else r"C:\Users\filip\AppData\Local\CrashDumps\salamand.exe.26504.dmp"
    parse_minidump(dump_path)
