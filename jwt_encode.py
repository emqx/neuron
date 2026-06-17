import jwt  # pip install pyjwt
import time
from pathlib import Path

key_candidates = [
    Path("config") / "neuron.key",
    Path("neuron.key"),
]

private_key = None
for key_path in key_candidates:
    if key_path.is_file():
        private_key = key_path.read_text()
        break

if private_key is None:
    raise FileNotFoundError("Cannot find neuron.key in ./config or the current directory")

current_time = int(time.time())
two_years = 60 * 60 * 24 * 365 * 2
header = {"alg": "RS256", "typ": "JWT"}
payload = {
    "aud": "neuron",
    "bodyEncode": 0,
    "exp": current_time + two_years,
    "iat": current_time,
    "iss": "neuron",
}
token = jwt.encode(payload, private_key, algorithm="RS256", headers=header).decode(
    "utf-8"
)
print(f"Bearer {token}")
