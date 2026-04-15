#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
LZW 压缩脚本：读取开局库文本文件，使用 LZW 算法压缩，输出 C++ 数组定义
"""

def lzw_compress(data):
    """
    标准 LZW 算法压缩
    :param data: 输入的字符串
    :return: 压缩后的整数列表（每个值为 uint16_t）
    """
    # 初始化字典，包含 0-255 的所有单字节字符
    dict_size = 256
    dictionary = {chr(i): i for i in range(dict_size)}
    
    result = []
    w = ""
    
    for c in data:
        wc = w + c
        if wc in dictionary:
            w = wc
        else:
            # 输出 w 的编码
            result.append(dictionary[w])
            # 将 wc 加入字典（如果字典还没满）
            if dict_size < 65536:  # uint16_t 的最大值
                dictionary[wc] = dict_size
                dict_size += 1
            # 重置 w 为当前字符
            w = c
    
    # 输出最后一个编码
    if w:
        result.append(dictionary[w])
    
    return result


def generate_cpp_array(compressed_data, line_width=20):
    """
    将压缩数据转换为 C++ 数组定义
    :param compressed_data: 压缩后的整数列表
    :param line_width: 每行输出的元素数量
    :return: C++ 代码字符串
    """
    lines = []
    
    for i in range(0, len(compressed_data), line_width):
        chunk = compressed_data[i:i+line_width]
        line = ", ".join(str(x) for x in chunk)
        lines.append("    " + line + ("," if i + line_width < len(compressed_data) else ""))
    
    cpp_code = "const std::vector<uint16_t> COMPRESSED_BOOK = {\n"
    cpp_code += "\n".join(lines)
    cpp_code += "\n};"
    
    return cpp_code


def main():
    input_file = "slim_book_v2.txt"
    output_file = "compressed_book.txt"
    
    print(f"[*] 读取文件: {input_file}")
    with open(input_file, 'r', encoding='utf-8') as f:
        raw_text = f.read()
    
    original_size = len(raw_text.encode('utf-8'))
    print(f"[*] 原始大小: {original_size:,} 字节")
    
    print("[*] 执行 LZW 压缩...")
    compressed_data = lzw_compress(raw_text)
    
    # 计算压缩后的大小（每个 uint16_t 2 字节）
    compressed_size = len(compressed_data) * 2
    compression_ratio = (1 - compressed_size / original_size) * 100
    
    print(f"[*] 压缩后元素个数: {len(compressed_data):,}")
    print(f"[*] 压缩后大小（作为数组）: {compressed_size:,} 字节")
    print(f"[*] 压缩率: {compression_ratio:.2f}%")
    
    print("[*] 生成 C++ 代码...")
    cpp_array_def = generate_cpp_array(compressed_data)
    
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(cpp_array_def)
    
    print(f"[*] C++ 数组定义已输出到: {output_file}")
    print(f"\n========== C++ 数组定义（前 500 字符）==========")
    print(cpp_array_def[:500] + "...\n")
    
    print("[*] 完成！")


if __name__ == "__main__":
    main()
