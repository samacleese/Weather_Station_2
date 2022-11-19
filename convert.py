from PIL import Image
import numpy as np

image = Image.open("/home/jesse/Arduino/Weather_Station_2/Pcals3.png")
data = np.asarray(image)

# Map the pallet to the bits
bits = {
    0: 0,
    1: 32,
    2: 96,
    3: 128,
    4: 160,
    5: 192,
    6: 224,
    7: 255,
}

hex_map = {
    0: "0",
    1: "2",
    2: "4",
    3: "6",
    4: "8",
    5: "a",
    6: "c",
    7: "e",
}
value_bits = {v: k for (k, v) in bits.items()}

p = np.split(np.asarray(image.getpalette()), 256)
p_map = {idx: p[idx] for idx in range(0, 8)}

f = lambda x: p_map[x]

p2 = [f(pix)[0] for pix in data.flatten()]
p3 = [value_bits[pix] for pix in p2]
p4 = [hex_map[pix] for pix in p3]

print(np.array([1, 2, 3]) == [1, 2, 3])
print(type(data.flatten()))
print(p4[0:10])
output = ""
idx = 0
for pix in p4:
    if idx == 0:
        output += "0x"
    output += pix
    if idx == 1:
        output += ","
    idx += 1
    idx %= 2

with open("pcals2.cpp", "w") as file:
    file.write(output)

print(output[0:100])
