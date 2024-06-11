import jwt  # pip install pyjwt
import time

with open("neuron.key", "r") as f:
    private_key = f.read()

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
