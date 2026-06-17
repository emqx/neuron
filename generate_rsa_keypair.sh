#!/bin/bash

set -euo pipefail

output_dir="${1:-config}"
private_key_file="${output_dir}/neuron.key"
public_key_file="${output_dir}/neuron.key.pub"

if ! command -v openssl >/dev/null 2>&1; then
  echo "openssl is required but was not found in PATH."
  exit 1
fi

mkdir -p "${output_dir}"

if [[ -f "${private_key_file}" || -f "${public_key_file}" ]]; then
  read -r -p "Existing key files found in ${output_dir}. Overwrite them? [y/N] " answer
  case "${answer}" in
    [yY]|[yY][eE][sS]) ;;
    *)
      echo "Aborted."
      exit 0
      ;;
  esac
fi

umask 077
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out "${private_key_file}"
openssl pkey -in "${private_key_file}" -pubout -out "${public_key_file}"

echo "Generated RSA key pair:"
echo "  ${private_key_file}"
echo "  ${public_key_file}"
echo "Run neuron with --config_dir ${output_dir} to use the generated keys."
