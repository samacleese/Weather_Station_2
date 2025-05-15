#!/usr/bin/env python3
import subprocess
import re
import os
import tempfile
import argparse
from pathlib import Path
import json
from collections import defaultdict


def get_certificates(hostname, port=443):
    """Extract certificates from a website using OpenSSL."""
    cmd = f"openssl s_client -showcerts -verify 5 -connect {hostname}:{port}"
    try:
        result = subprocess.run(
            cmd.split(),
            input="",
            capture_output=True,
            text=True,
            check=True
        )
        return extract_certs_from_output(result.stdout)
    except subprocess.CalledProcessError as e:
        print(f"Error connecting to {hostname}:{port}")
        print(e.stderr)
        return {}


def extract_certs_from_output(output):
    """Extract certificates and their details from OpenSSL output."""
    certs = {}
    current_cert = []
    in_cert = False
    cert_num = 0

    for line in output.splitlines():
        if "-----BEGIN CERTIFICATE-----" in line:
            in_cert = True
            current_cert = [line]
        elif "-----END CERTIFICATE-----" in line:
            in_cert = False
            current_cert.append(line)
            cert_text = "\n".join(current_cert)

            # Write to temp file to get details
            with tempfile.NamedTemporaryFile(delete=False) as temp:
                temp.write(cert_text.encode())
                temp_name = temp.name

            try:
                # Get certificate details
                cert_info = get_cert_info(temp_name)
                cert_name = cert_info.get('cn', f'cert{cert_num}')
                # Clean the name
                cert_name = re.sub(r'[ ,.*]', '_', cert_name)
                cert_name = re.sub(r'__', '_', cert_name)
                cert_name = re.sub(r'_-_', '-', cert_name)
                cert_name = re.sub(r'^_', '', cert_name)
                cert_name = cert_name.lower()

                certs[cert_name] = {
                    'text': cert_text,
                    'info': cert_info
                }
                cert_num += 1
            finally:
                os.unlink(temp_name)
        elif in_cert:
            current_cert.append(line)

    return certs


def get_cert_info(cert_file):
    """Extract subject, issuer, and SANs from a certificate."""
    info = {}

    # Get subject
    cmd = f"openssl x509 -noout -subject -in {cert_file}"
    result = subprocess.run(cmd.split(), capture_output=True, text=True)
    if result.returncode == 0:
        match = re.search(r'CN ?= ?([^,/]+)', result.stdout)
        if match:
            info['cn'] = match.group(1)

    # Get issuer
    cmd = f"openssl x509 -noout -issuer -in {cert_file}"
    result = subprocess.run(cmd.split(), capture_output=True, text=True)
    if result.returncode == 0:
        match = re.search(r'CN ?= ?([^,/]+)', result.stdout)
        if match:
            info['issuer_cn'] = match.group(1)

    # Get Subject Alternative Names
    cmd = f"openssl x509 -noout -text -in {cert_file}"
    result = subprocess.run(cmd.split(), capture_output=True, text=True)
    if result.returncode == 0:
        sans = []
        # Find the DNS entries in the SAN section
        san_section = re.search(r'X509v3 Subject Alternative Name:[^\n]*\n\s*(.*?)\n',
                               result.stdout, re.DOTALL)
        if san_section:
            # Extract DNS names
            dns_names = re.findall(r'DNS:([^,\s]+)', san_section.group(1))
            sans.extend(dns_names)
        info['sans'] = sans

    return info


def find_root_ca(certs):
    """Find the root CA certificate from a collection of certificates.

    A root CA typically has the same issuer and subject (self-signed).
    """
    for cert_name, cert_data in certs.items():
        # Get issuer and subject CN
        issuer_cn = cert_data['info'].get('issuer_cn')
        subject_cn = cert_data['info'].get('cn')

        # If issuer and subject are the same, it's likely a root CA
        if issuer_cn and subject_cn and issuer_cn == subject_cn:
            return cert_name

    # Fallback: Return the last certificate in the chain
    return list(certs.keys())[-1] if certs else None


def format_cert_for_cpp(cert_text):
    """Format certificate text for inclusion in C++ code."""
    # Split by lines and wrap in quotes
    lines = cert_text.strip().split('\n')
    quoted_lines = [f'    "{line}\\n"' for line in lines]
    return '\n'.join(quoted_lines)


def generate_cpp_file(domains_to_certs, output_file="CACerts.h"):
    """Generate a C++ header file with certificates."""
    unique_certs = {}
    domain_to_cert_var = {}

    # First, identify unique certificates and create variables for them
    cert_counter = defaultdict(int)
    for domain, cert_name in domains_to_certs.items():
        cert_var_base = re.sub(r'[^a-z0-9_]', '_', cert_name.lower())
        if cert_var_base not in unique_certs:
            # Count occurrences to handle duplicates
            count = cert_counter[cert_var_base]
            cert_counter[cert_var_base] += 1

            if count > 0:
                cert_var = f"{cert_var_base}_{count}"
            else:
                cert_var = cert_var_base

            unique_certs[cert_var] = cert_name
        else:
            # If we've already stored this cert, use the existing variable name
            for var, name in unique_certs.items():
                if name == cert_name:
                    cert_var = var
                    break

        domain_to_cert_var[domain] = cert_var

    # Now generate the C++ file
    with open(output_file, 'w') as f:
        f.write("#pragma once\n\n")
        f.write("#include <map>\n")
        f.write("#include <Arduino.h>\n\n")
        f.write("namespace CACerts {\n\n")

        # Write each unique certificate
        f.write("namespace {\n")
        for cert_var, cert_name in unique_certs.items():
            # Read the certificate file
            try:
                with open(f"{cert_name}.pem", 'r') as cert_file:
                    cert_text = cert_file.read()

                # Format and write
                f.write(f"const char PROGMEM {cert_var}[] =\n")
                f.write(format_cert_for_cpp(cert_text))
                f.write(";\n\n")
            except FileNotFoundError:
                print(f"Warning: Certificate file {cert_name}.pem not found")
        f.write("}\n\n")

        # Write the map of domains to certificates
        f.write("const std::map<String, const char *> ca_certs{\n")
        for domain, cert_var in sorted(domain_to_cert_var.items()):
            padding = " " * (40 - len(domain))
            f.write(f'    {{"{domain}",{padding} {cert_var}}},\n')
        f.write("};\n")

        f.write("}\n")  # End namespace


def process_domain_list(domain_list, output_file="CACerts.h"):
    """Process a list of domains and generate the certificate map."""
    domains_to_certs = {}

    for domain in domain_list:
        print(f"Processing domain: {domain}")
        certs = get_certificates(domain)

        if certs:
            # Try to find the root CA
            root_ca_name = find_root_ca(certs)
            
            # If not found, use the last certificate in the chain
            if not root_ca_name:
                root_ca_name = list(certs.keys())[-1]
                
            domains_to_certs[domain] = root_ca_name

            # Save the certificate to a file
            with open(f"{root_ca_name}.pem", 'w') as f:
                f.write(certs[root_ca_name]['text'])

            print(f"  → Found root CA: {root_ca_name}")
        else:
            print(f"  → No certificates found")

    # Generate the C++ file
    generate_cpp_file(domains_to_certs, output_file)
    print(f"\nCreated {output_file} with certificates for {len(domains_to_certs)} domains")


def extract_domains_from_cpp(cpp_file):
    """Extract domains from an existing C++ certificate file."""
    domains = []
    with open(cpp_file, 'r') as f:
        content = f.read()

    # Find all the domains in the map
    for match in re.finditer(r'{"([^"]+)"', content):
        domains.append(match.group(1))

    return domains


def main():
    parser = argparse.ArgumentParser(description='Generate SSL certificate mapping for Arduino projects')
    parser.add_argument('-f', '--file', help='Path to save the output C++ header file', default="CACerts.h")
    parser.add_argument('-d', '--domains', help='Comma-separated list of domains', default=None)
    parser.add_argument('-i', '--input-file', help='File containing list of domains (one per line)', default=None)
    parser.add_argument('-u', '--update', help='Update certificates from existing C++ file', default=None)

    args = parser.parse_args()

    domains = []

    # Get domains from various sources
    if args.domains:
        domains.extend(args.domains.split(','))

    if args.input_file:
        with open(args.input_file, 'r') as f:
            domains.extend([line.strip() for line in f if line.strip()])

    if args.update:
        domains.extend(extract_domains_from_cpp(args.update))

    if not domains:
        parser.error("No domains specified. Use --domains, --input-file, or --update")
        return

    # Remove duplicates while preserving order
    domains = list(dict.fromkeys(domains))

    # Process the domains
    process_domain_list(domains, args.file)


if __name__ == "__main__":
    main()
