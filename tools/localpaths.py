"""Machine-specific paths for the development tools, kept out of the repository.

Values come from local/paths.json (not tracked) or from environment variables of the same name in upper case
with a DDS_ prefix (DDS_TEST_PCSX2, DDS_ISO, DDS_USER_PCSX2, DDS_NUDGE_OUT). Example local/paths.json:
    {"test_pcsx2": "<folder of a portable PCSX2 used for testing>", "iso": "<the game's disc image>",
     "user_pcsx2": "<your PCSX2 data folder>", "nudge_out": "<file for nudge.py's output>"}
"""
import json, os

_HERE = os.path.dirname(os.path.abspath(__file__))
_FILE = os.path.join(_HERE, '..', 'local', 'paths.json')


def get(key, required=True):
    v = os.environ.get('DDS_' + key.upper())
    if not v and os.path.exists(_FILE):
        v = json.load(open(_FILE, encoding='utf-8')).get(key)
    if not v and required:
        raise SystemExit('set "%s" in local/paths.json or DDS_%s (see tools/localpaths.py)' % (key, key.upper()))
    return v


if __name__ == '__main__':          # used by the PowerShell tools: python localpaths.py key
    import sys
    print(get(sys.argv[1]))
