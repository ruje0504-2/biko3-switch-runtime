"""Enforce dependency direction and keep native APIs behind the backend boundary."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parent.parent
ALLOWED = {
    'core': {'core'},
    'save': {'save', 'resource'},
    'resource': {'resource'},
    'media': {'media', 'core', 'resource'},
    'model': {'model', 'core', 'resource'},
    'world': {'world', 'core', 'model'},
    'game': {'game', 'world', 'core', 'resource'},
    'platform': {'platform', 'core', 'media'},
    'render': {'render', 'resource', 'core'},
    'ui': {'ui', 'resource'},
    'scene': {'scene', 'core', 'resource', 'render', 'ui', 'model', 'game', 'world', 'media'},
    'app': {'app', 'core', 'resource', 'platform', 'render', 'scene', 'media', 'save'},
}


class Architecture(unittest.TestCase):
    def test_dependency_direction(self):
        for path in (ROOT/'runtime').rglob('*'):
            if path.suffix not in ('.c', '.h'):
                continue
            module = path.relative_to(ROOT/'runtime').parts[0]
            self.assertIn(module, ALLOWED, f'{path}: add an explicit dependency rule')
            for included in re.findall(r'^#include "([^"]+)"', path.read_text(), re.M):
                if included in ('shaders.h', 'build_info.h'):
                    continue
                dependency = included.split('/')[0]
                self.assertIn(dependency, ALLOWED[module], f'{path}: {included}')
                if included.endswith('_internal.h'):
                    self.assertEqual(module, dependency, f'private API leaked: {path}')

    def test_native_api_isolation(self):
        for path in (ROOT/'runtime').rglob('*'):
            if path.suffix not in ('.c', '.h'):
                continue
            relative = path.relative_to(ROOT/'runtime').as_posix()
            text = path.read_text()
            if '#include <switch.h>' in text or '__SWITCH__' in text:
                self.assertTrue(relative in ('platform/switch.c', 'platform/audio_switch.c') or
                                relative.startswith('render/vulkan/'), relative)
            if re.search(r'#include <vulkan/|\bVk[A-Z]\w+', text):
                self.assertTrue(relative.startswith('render/vulkan/'), relative)


if __name__ == '__main__':
    unittest.main()
