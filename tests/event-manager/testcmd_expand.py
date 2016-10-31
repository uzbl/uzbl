from uzbl.plugins.cmd_expand import cmd_expand


def test_escape():
    cases = [
        ("c'thulu", "'c\\'thulu'"),
        ("'compli\\'cated'", "'\\'compli\\\\\\'cated\\''"),
    ]
    for input, expect in cases:
        output = cmd_expand("%r", [input])
        assert output == expect
