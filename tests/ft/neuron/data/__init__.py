from base64 import b64encode
import pathlib


def base64(file):
    with open(file, "rb") as f:
        return b64encode(f.read()).decode("utf-8")


data_dir = pathlib.Path(__file__).resolve().parent
ca = str(data_dir / "ca.pem")
ca_base64 = base64(ca)
server_cert = str(data_dir / "server.pem")
server_cert_base64 = base64(server_cert)
server_key = str(data_dir / "server.key")
server_key_base64 = base64(server_key)
client_cert = str(data_dir / "client.pem")
client_cert_base64 = base64(client_cert)
client_key = str(data_dir / "client.key")
client_key_base64 = base64(client_key)
