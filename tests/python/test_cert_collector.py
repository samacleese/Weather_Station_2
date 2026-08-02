# ABOUTME: Tests for the pure parsing/formatting logic in tools/cert_collector.py.
# ABOUTME: Functions that shell out to openssl (get_certificates, get_cert_info) are not covered here.
from cert_collector import extract_certs_from_output, find_root_ca, format_cert_for_cpp

FAKE_CERT_1 = "-----BEGIN CERTIFICATE-----\nMIIB1\n-----END CERTIFICATE-----"
FAKE_CERT_2 = "-----BEGIN CERTIFICATE-----\nMIIB2\n-----END CERTIFICATE-----"


def test_extract_certs_from_output_splits_multiple_certs(monkeypatch):
    # get_cert_info shells out to openssl; stub it so this test only exercises parsing.
    import cert_collector

    names = iter(["leaf", "root"])
    monkeypatch.setattr(cert_collector, "get_cert_info", lambda path: {"cn": next(names)})

    openssl_output = f"junk before\n{FAKE_CERT_1}\nmore junk\n{FAKE_CERT_2}\ntrailing"
    certs = extract_certs_from_output(openssl_output)

    assert list(certs.keys()) == ["leaf", "root"]
    assert certs["leaf"]["text"] == FAKE_CERT_1
    assert certs["root"]["text"] == FAKE_CERT_2


def test_extract_certs_from_output_sanitizes_cn_into_safe_filename(monkeypatch):
    import cert_collector

    monkeypatch.setattr(cert_collector, "get_cert_info", lambda path: {"cn": "Example*Wildcard!!"})

    certs = extract_certs_from_output(FAKE_CERT_1)

    assert list(certs.keys()) == ["example_wildcard"]


def test_extract_certs_from_output_falls_back_to_generated_name(monkeypatch):
    import cert_collector

    monkeypatch.setattr(cert_collector, "get_cert_info", lambda path: {})

    certs = extract_certs_from_output(FAKE_CERT_1)

    assert list(certs.keys()) == ["cert0"]


def test_find_root_ca_prefers_self_signed_cert():
    certs = {
        "leaf": {"info": {"cn": "example.com", "issuer_cn": "Intermediate CA"}},
        "root": {"info": {"cn": "Root CA", "issuer_cn": "Root CA"}},
    }
    assert find_root_ca(certs) == "root"


def test_find_root_ca_falls_back_to_last_cert_when_none_self_signed():
    certs = {
        "leaf": {"info": {"cn": "example.com", "issuer_cn": "Intermediate CA"}},
        "intermediate": {"info": {"cn": "Intermediate CA", "issuer_cn": "Root CA"}},
    }
    assert find_root_ca(certs) == "intermediate"


def test_find_root_ca_returns_none_for_empty_input():
    assert find_root_ca({}) is None


def test_format_cert_for_cpp_wraps_each_line_in_quotes_with_newline():
    result = format_cert_for_cpp(FAKE_CERT_1)
    lines = result.split("\n")
    assert lines[0] == '    "-----BEGIN CERTIFICATE-----\\n"'
    assert lines[1] == '    "MIIB1\\n"'
    assert lines[2] == '    "-----END CERTIFICATE-----\\n"'
