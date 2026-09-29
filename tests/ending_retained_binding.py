"""ctypes layout of recovered retained scalars, with independent x86 mapping."""
import ctypes as C

I, B, F = C.c_int32, C.c_uint8, C.c_float


class Normal(C.Structure):
    _fields_ = [(name, I) for name in (
        'word_719b24', 'word_719448', 'word_719b28', 'word_719b2c',
        'word_719b40', 'word_719b50')] + [('words_719b54', I * 3)]


class Stage3(C.Structure):
    _fields_ = [('words_6bbe2c', I * 2), ('byte_6bbe34', B)]


class Auxiliary(C.Structure):
    _fields_ = [('word_6ea16c', I), ('group_prefix', (I * 3) * 5),
                ('group_suffix', (I * 12) * 5), ('words_6ea2c0', I * 20),
                ('word_6ea314', I), ('word_5546a0', I),
                ('words_6ea028', I * 75), ('words_6ea318', I * 10)] + [
        (name, I) for name in ('word_6ea340', 'word_6ddec0', 'word_6ddec8',
                              'word_6dde90', 'word_6ea348', 'word_6ea354',
                              'word_6ea020')] + [('byte_6ea358', B)]


class Stage2(C.Structure):
    _fields_ = [('word_6a3c20', I), ('words_6afcfc', I * 3),
                ('word_6afd08', I), ('word_6a3c24', I),
                ('word_54ccc8', I), ('byte_6afd18', B), ('value_54ccd0', F)]


class Stage4(C.Structure):
    _fields_ = [('words_6c7f44', I * 2), ('bytes_6c7f54', B * 10),
                ('bytes_6c7f60', B * 10), ('word_54e2f8', I),
                ('word_6c7f4c', I), ('byte_6c7f50', B), ('value_54e310', F)]


class Final(C.Structure):
    _fields_ = [(name, B) for name in (
        'byte_6c7f70', 'byte_6d1be0', 'byte_6d1bd4', 'byte_6d1c0d',
        'byte_6ddce0', 'byte_6d1be1')] + [
        ('word_6c7f74', I), ('word_6dde4c', I), ('word_6d1bcc', I),
        ('byte_6dde50', B), ('byte_6dde51', B), ('word_6d1bd8', I),
        ('word_6d1bdc', I), ('word_6dde54', I), ('byte_6dde58', B),
        ('workspace_6c7f80', I * 10000), ('words_6dde24', I * 10),
        ('words_6d1be8', I * 9), ('words_6ddce4', I * 75)]


class Retained(C.Structure):
    _fields_ = [('normal', Normal), ('stage3', Stage3),
                ('auxiliary', Auxiliary), ('stage2', Stage2),
                ('stage4', Stage4), ('final', Final)]


def regions(base):
    result = []
    for group, typ in Retained._fields_:
        group_base = base + getattr(Retained, group).offset
        for name, field_type in typ._fields_:
            offset = group_base + getattr(typ, name).offset
            label = 'retained.' + group + '.' + name
            if name in ('group_prefix', 'group_suffix'):
                length = 12 if name == 'group_prefix' else 48
                address = 0x6ea180 if name == 'group_prefix' else 0x6ea190
                result.extend((offset + i * length, address + i * 64,
                               length, label + str(i)) for i in range(5))
            else:
                result.append((offset, int(name.split('_')[-1], 16),
                               C.sizeof(field_type), label))
    return result
