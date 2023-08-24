import langtable

import locale
import re

if hasattr(langtable, 'list_all_keyboards'):
    keyboards = langtable.list_all_keyboards()
else:
    from langtable.langtable import _keyboards_db
    keyboards = _keyboards_db.keys()

non_latin_keyboards = {}

for keyboard in keyboards:
    # Check if the keyboard supports ASCII
    if not langtable.supports_ascii(keyboardId=keyboard):
        input_source = re.sub(r'\((.*?)\)', r'+\1', keyboard)
        non_latin_keyboards[input_source] = 'xkb'

sorted_non_latin_keyboards = sorted(non_latin_keyboards.items(), key=lambda x: x[0])

header_prolog = '''
typedef struct
{
  char *type;
  char *id;
} InputSource;

static InputSource non_latin_input_sources[] =
{
'''

header_epilog = '''
};
'''

with open('non-latin-input-sources.h', 'w') as file:
    file.write(header_prolog)

    for keyboard, type in sorted_non_latin_keyboards:
        file.write(f'  {{ "{type}", "{keyboard}" }},\n')
    file.write("  { NULL, NULL },")

    file.write(header_epilog)
