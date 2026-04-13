bits_hex = '39667ffc0da840'
bits_bytes = bytes.fromhex(bits_hex)
bits = ''.join(format(b,'08b') for b in bits_bytes)
pos = [0]
def read(n):
    v = int(bits[pos[0]:pos[0]+n],2) if n>0 else 0
    pos[0]+=n
    return v

seq_profile = read(3)
still_picture = read(1)
reduced = read(1)
seq_level_idx = read(5)
# only reduced still picture header path for this file
n1 = read(4)
n2 = read(4)
max_w = read(n1+1)+1
max_h = read(n2+1)+1
print("pos after frame size:",pos[0],"max_w=%d max_h=%d"%(max_w,max_h))
use128 = read(1)
ef_filter_intra = read(1)
ef_intra_edge = read(1)
print("use128=%d filter_intra=%d intra_edge=%d pos=%d"%(use128,ef_filter_intra,ef_intra_edge,pos[0]))
# inter tools not needed for still picture (still_picture=1, reduced=1)
# seq_choose_screen_content_tools
seq_choose_screen_content_tools = read(1)
if seq_choose_screen_content_tools:
    seq_force_screen_content_tools = 2
else:
    seq_force_screen_content_tools = read(1)
print("seq_force_sct=%d pos=%d"%(seq_force_screen_content_tools,pos[0]))
if seq_force_screen_content_tools > 0:
    seq_choose_int_mv = read(1)
    print("seq_choose_int_mv=%d pos=%d"%(seq_choose_int_mv,pos[0]))
# order_hint: enable_order_hint=0 for this file
# high_bitdepth
hbd = read(1)
print("high_bitdepth=%d pos=%d"%(hbd,pos[0]))
