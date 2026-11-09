"""Wide logical registers use real native words and retain high-bit coherence."""

import math

import pytest

import pysparq as ps
from pysparq.register_layout import RegisterLayout

def register(name, width):
    return {"tag": "Register", "name": name, "type": {"tag": "RegType", "kind": "bits", "width": width}}


def ref(name, start, width):
    return {"tag": "Ref", "parts": [{"tag": "Span", "register": name, "start": start, "width": width}],
            "type": {"tag": "RegType", "kind": "bits", "width": width}}


def whole(name, width):
    return ref(name, 0, width)


def prim(op, operands, value=None, angle=None):
    return {"tag": "Primitive", "op": op, "operands": operands, "value": value, "angle": angle}


def module(name, registers, body, locals_=()):
    return {"tag": "Module", "name": name, "registers": registers, "body": body,
            "locals": list(locals_), "resources": [], "attributes": []}


def program(entry, modules):
    return {"tag": "Program", "entry": entry, "modules": modules, "version": "0.3"}


@pytest.mark.parametrize("width", [64, 65, 127, 128, 129, 256])
def test_native_words_preserve_high_bit_and_relative_phase(width):
    ps.System.clear()
    try:
        state = ps.SparseState()
        layout = RegisterLayout(state)
        bits = layout.add("wide", ps.StateStorageType.General, width)
        h = [1/math.sqrt(2), 1/math.sqrt(2), 1/math.sqrt(2), -1/math.sqrt(2)]
        z = [1, 0, 0, -1]
        ps.Rot_Bool(*bits[-1], h)(state)
        ps.Rot_Bool(*bits[-1], z)(state)
        actual = {layout.value("wide", basis): complex(basis.amplitude) for basis in state.basis_states}
        assert actual == {0: pytest.approx(1/math.sqrt(2)),
                          1 << (width-1): pytest.approx(-1/math.sqrt(2))}
        ps.Rot_Bool(*bits[-1], z)(state)
        ps.Rot_Bool(*bits[-1], h)(state)
        layout.remove("wide")
        assert ps.System.get_activated_register_size() == 0
    finally:
        ps.System.clear()


def test_rir_large_integer_carries_cross_word_boundaries():
    width = 130
    amount = (1 << 96)+3
    body = [prim("add_const", [whole("q", width)], value=(1 << 129)-1),
            prim("add_const", [whole("q", width)], value=amount)]
    expected = ((1 << 129)-1+amount) % (1 << width)
    body.append({"tag": "Control", "register": ref("q", 64, 66), "value": expected >> 64,
                 "body": [prim("x", [whole("out", 1)])]})
    doc = program("main", [module("main", [register("q", width), register("out", 1)], body)])
    assert ps.run_rir(doc).amplitudes == {(expected, 1): pytest.approx(1)}


def test_wide_local_cleanup_rejects_nonzero_high_words():
    doc = program("main", [module("main", [register("q", 1)],
        [prim("x", [ref("scratch", 128, 1)])], locals_=[register("scratch", 129)])])
    with pytest.raises(ps.RIRError, match="not uncomputed"):
        ps.run_rir(doc)
    assert ps.System.get_activated_register_size() == 0


def test_wide_qram_keeps_high_address_data_and_zero_address_controls():
    resource = {"tag": "Resource", "name": "bank", "type": {
        "tag": "QRAM", "address_width": 65, "data_width": 129}}
    load = {"tag": "Load", "resource": "bank", "address": whole("address", 65), "data": whole("data", 129)}
    body = [prim("h", [ref("address", 64, 1)]), {"tag": "Control", "register": ref("address", 64, 1),
             "value": 0, "body": [load]}]
    main = module("main", [register("address", 65), register("data", 129)], body)
    main["resources"] = [resource]
    memory = {"bank": {0: (1 << 128)+3, 1 << 64: (1 << 65)+1}}
    result = ps.run_rir(program("main", [main]), memory)
    assert result.amplitudes == {(0, (1 << 128)+3): pytest.approx(2**-.5),
                                 (1 << 64, 0): pytest.approx(2**-.5)}


def test_high_bit_entanglement_across_word_boundaries():
    body = [prim("h", [ref("q", 0, 1)]),
            {"tag": "Control", "register": ref("q", 0, 1), "value": 1,
             "body": [prim("x", [ref("q", 129, 1)])]}]
    doc = program("main", [module("main", [register("q", 130)], body)])
    result = ps.run_rir(doc)
    a = math.sqrt(0.5)
    assert result.amplitudes == {(0,): pytest.approx(a), ((1 << 129) | 1,): pytest.approx(a)}
    assert ps.System.get_activated_register_size() == 0


def test_controlled_global_phase_on_high_word_bit():
    body = [prim("h", [ref("q", 128, 1)]),
            {"tag": "Control", "register": ref("q", 128, 1), "value": 1,
             "body": [prim("gphase", [], angle=math.pi / 2)]}]
    doc = program("main", [module("main", [register("q", 130)], body)])
    result = ps.run_rir(doc)
    a = math.sqrt(0.5)
    assert result.amplitudes == {(0,): pytest.approx(a), (1 << 128,): pytest.approx(1j * a)}
    assert ps.System.get_activated_register_size() == 0


def test_view_slices_span_word_boundaries_xor_swap():
    pattern = (1 << 9) | (1 << 4)  # offsets inside the 10-bit slices
    body = [
        prim("add_const", [ref("q", 60, 10)], value=pattern),
        prim("xor", [ref("q", 60, 10), ref("r", 60, 10)]),
        prim("swap", [ref("q", 60, 10), ref("r", 0, 10)]),
    ]
    doc = program("main", [module("main", [register("q", 130), register("r", 70)], body)])
    result = ps.run_rir(doc)
    expected_r = pattern | (pattern << 60)
    assert result.amplitudes == {(0, expected_r): pytest.approx(1)}


def test_multi_span_view_concatenates_across_words():
    span = {"tag": "Ref", "parts": [
        {"tag": "Span", "register": "q", "start": 0, "width": 2},
        {"tag": "Span", "register": "q", "start": 128, "width": 2}],
        "type": {"tag": "RegType", "kind": "bits", "width": 4}}
    body = [prim("x", [ref("q", 1, 1)]), prim("x", [ref("q", 129, 1)]),
            prim("xor", [span, ref("r", 0, 4)])]
    doc = program("main", [module("main", [register("q", 130), register("r", 4)], body)])
    result = ps.run_rir(doc)
    # view value = bit0*1 + bit1*2 + bit128*4 + bit129*8 = 2 + 8
    assert result.amplitudes == {((1 << 1) | (1 << 129), 10): pytest.approx(1)}


def test_wide_add_const_wraps_at_full_logical_width():
    body = [prim("add_const", [whole("q", 130)], value=(1 << 130) - 1),
            prim("add_const", [whole("q", 130)], value=1)]
    doc = program("main", [module("main", [register("q", 130)], body)])
    assert ps.run_rir(doc).amplitudes == {(0,): pytest.approx(1)}


def test_wide_add_const_view_wrap_crosses_word_boundary():
    body = [prim("add_const", [ref("q", 60, 10)], value=(1 << 10) - 1),
            prim("add_const", [ref("q", 60, 10)], value=1)]
    doc = program("main", [module("main", [register("q", 130)], body)])
    assert ps.run_rir(doc).amplitudes == {(0,): pytest.approx(1)}


def test_adjoint_add_const_restores_nonzero_wide_state():
    keep = (1 << 127) + 5
    swing = (1 << 96) + 3
    body = [
        prim("add_const", [whole("q", 130)], value=keep),
        prim("add_const", [whole("q", 130)], value=swing),
        {"tag": "Adjoint", "body": [prim("add_const", [whole("q", 130)], value=swing)]},
    ]
    doc = program("main", [module("main", [register("q", 130)], body)])
    assert ps.run_rir(doc).amplitudes == {(keep,): pytest.approx(1)}


def test_wide_workspace_nonzero_then_uncomputed():
    body = [
        prim("x", [ref("scratch", 100, 1)]),
        {"tag": "Control", "register": ref("scratch", 100, 1), "value": 1,
         "body": [prim("x", [whole("out", 1)])]},
        prim("x", [ref("scratch", 100, 1)]),
    ]
    doc = program("main", [module("main", [register("out", 1)],
                                  body, locals_=[register("scratch", 129)])])
    assert ps.run_rir(doc).amplitudes == {(1,): pytest.approx(1)}
    assert ps.System.get_activated_register_size() == 0


def _wide_qram_doc(load_once):
    resource = {"tag": "Resource", "name": "bank", "type": {
        "tag": "QRAM", "address_width": 65, "data_width": 129}}
    load = {"tag": "Load", "resource": "bank",
            "address": whole("address", 65), "data": whole("data", 129)}
    # Zero and one controls share the address register across the word-63/64
    # boundary: bit 63 must be 1 and bit 64 must be 0 for the query to fire.
    control = {"tag": "Control", "register": ref("address", 63, 2), "value": 0b01,
               "body": [load]}
    body = [prim("h", [ref("address", 0, 1)]), prim("h", [ref("address", 63, 1)]),
            prim("add_const", [whole("data", 129)], value=(1 << 128) + 2), control]
    if not load_once:
        body.append({"tag": "Adjoint", "body": [control]})
    main = module("main", [register("address", 65), register("data", 129)], body)
    main["resources"] = [resource]
    return program("main", [main])


def test_wide_qram_xor_onto_nonzero_data_with_mixed_address_controls():
    memory = {"bank": {0: 1, 1: 3, 1 << 63: (1 << 100) + 7, (1 << 63) | 1: 5}}
    result = ps.run_rir(_wide_qram_doc(load_once=True), memory)
    initial = (1 << 128) + 2
    a = 0.5
    assert result.amplitudes == {
        (0, initial): pytest.approx(a),
        (1, initial): pytest.approx(a),
        (1 << 63, initial ^ ((1 << 100) + 7)): pytest.approx(a),
        ((1 << 63) | 1, initial ^ 5): pytest.approx(a),
    }


def test_wide_qram_unquery_restores_nonzero_data():
    memory = {"bank": {0: 1, 1: 3, 1 << 63: (1 << 100) + 7, (1 << 63) | 1: 5}}
    result = ps.run_rir(_wide_qram_doc(load_once=False), memory)
    initial = (1 << 128) + 2
    a = 0.5
    assert result.amplitudes == {
        (address, initial): pytest.approx(a)
        for address in (0, 1, 1 << 63, (1 << 63) | 1)
    }


def test_sparse_and_dense_native_qram_paths_agree():
    cells = {0: 5, 1: 2, 3: 7}

    def doc(address_width):
        resource = {"tag": "Resource", "name": "bank", "type": {
            "tag": "QRAM", "address_width": address_width, "data_width": 5}}
        body = [prim("h", [ref("address", 0, 1)]), prim("h", [ref("address", 1, 1)]),
                {"tag": "Load", "resource": "bank",
                 "address": whole("address", address_width), "data": whole("data", 5)}]
        main = module("main", [register("address", address_width), register("data", 5)], body)
        main["resources"] = [resource]
        return program("main", [main])

    dense = ps.run_rir(doc(4), memory={"bank": cells})       # native qutrit path
    sparse = ps.run_rir(doc(21), memory={"bank": cells})     # sparse word path
    assert dense.amplitudes and len(dense.amplitudes) == len(sparse.amplitudes)
    for (address, word), amplitude in sparse.amplitudes.items():
        assert dense.amplitudes[(address, word)] == pytest.approx(amplitude)


def test_sparse_qram_never_allocates_the_full_address_space():
    resource = {"tag": "Resource", "name": "bank", "type": {
        "tag": "QRAM", "address_width": 40, "data_width": 64}}
    target = (1 << 39) + 3
    body = [prim("add_const", [whole("address", 40)], value=target),
            {"tag": "Load", "resource": "bank",
             "address": whole("address", 40), "data": whole("data", 64)}]
    main = module("main", [register("address", 40), register("data", 64)], body)
    main["resources"] = [resource]
    memory = {"bank": {0: 9, target: 11}}
    result = ps.run_rir(program("main", [main]), memory)
    assert result.amplitudes == {(target, 11): pytest.approx(1)}


def test_wide_qram_rejects_view_width_mismatch():
    resource = {"tag": "Resource", "name": "bank", "type": {
        "tag": "QRAM", "address_width": 65, "data_width": 8}}
    body = [{"tag": "Load", "resource": "bank",
             "address": whole("address", 64), "data": whole("data", 8)}]
    main = module("main", [register("address", 64), register("data", 8)], body)
    main["resources"] = [resource]
    memory = {"bank": {0: 1}}
    with pytest.raises(ps.RIRError, match="QRAM"):
        ps.run_rir(program("main", [main]), memory)
    assert ps.System.get_activated_register_size() == 0


def test_register_layout_rejects_native_word_name_collisions():
    ps.System.clear()
    try:
        state = ps.SparseState()
        layout = RegisterLayout(state)
        layout.add("a", ps.StateStorageType.General, 100)  # allocates a__word_0/1
        with pytest.raises(ValueError, match="already in use"):
            layout.add("a__word_0", ps.StateStorageType.General, 10)
        foreign = RegisterLayout(state)
        with pytest.raises(ValueError, match="already in use"):
            foreign.add("a__word_1", ps.StateStorageType.General, 10)
        derived = RegisterLayout(state)
        derived.add("b__word_0", ps.StateStorageType.General, 10)
        with pytest.raises(ValueError, match="already in use"):
            derived.add("b", ps.StateStorageType.General, 100)  # derives b__word_0
        derived.remove("b__word_0")
        layout.remove("a", require_zero=True)
        assert ps.System.get_activated_register_size() == 0
    finally:
        ps.System.clear()


def test_register_layout_value_and_duplicate_rejection():
    ps.System.clear()
    try:
        state = ps.SparseState()
        layout = RegisterLayout(state)
        bits = layout.add("wide", ps.StateStorageType.General, 100)
        with pytest.raises(ValueError, match="already exists"):
            layout.add("wide", ps.StateStorageType.General, 3)
        with pytest.raises(ValueError):
            layout.add("bad", ps.StateStorageType.General, -1)
        high = bits[99]
        ps.X_Bool(*high)(state)
        assert layout.value("wide", state.basis_states[0]) == 1 << 99
        with pytest.raises(ValueError, match="not uncomputed"):
            layout.remove("wide")
        ps.X_Bool(*high)(state)
        layout.remove("wide")
        assert ps.System.get_activated_register_size() == 0
    finally:
        ps.System.clear()
