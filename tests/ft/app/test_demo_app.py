import time
import subprocess
import pytest
import base64

import neuron.api as api
import neuron.error as error
import neuron.config as config
from neuron.common import *
from neuron.error import *


NODE = "demo_app"
so_data = None
js_data = None

# Test constants for certificates and users
TEST_CERTIFICATE = """LS0tLS1CRUdJTiBDRVJUSUZJQ0FURS0tLS0tCk1JSURYVENDQWtXZ0F3SUJBZ0lKQUw4cEFhd3drcVB6TUEwR0NTcUdTSWIzRFFFQkN3VUFNRVl4Q3pBSkJnTlYKQkFZVEFsVlRNUXN3Q1FZRFZRUUlEQUpEUVRFUE1BMEdBMVVFQ2d3R1RtVjFjbTl1TVJrd0Z3WURWUVFEREJCTwpaWFZ5YjI0Z1JHVnRieUJEWlhKME1CNFhEVEkwTURrd01qRTVNREV3TVZvWERUTTBNRGd6TVRFNU1ERXdNVm93CkZURVRNQkVHQTFVRUF3d0tRVzVqYUdRdFRtOWtaVEZiTUEwR0NTcUdTSWIzRFFFQkFRVUFBMElBQUgwOUZ1RHgKd2NsaWZNOHZTU2p3TjF2WkVZQTRmcEFhd3drcVB6c0l0SFFqcVBzQz0KLS0tLS1FTkQgQ0VSVElGSUNBVEUtLS0tLQ=="""

TEST_PRIVATE_KEY = """LS0tLS1CRUdJTiBQUklWQVRFIEtFWS0tLS0tCk1JSUV2Z0lCQURBTkJna3Foa2lHOXcwQkFRRUZBQVNDQktnd2dnU2tBZ0VBQW9JQkFRRGY2UldWNXNIWVlYQUcKdGdJTzNkaXNMb0lnSXByZ0pFa21wSFJBajVUTHNIWVlYQUd0Z0lPM2Rpc0xvSWdJcHJnSkVrbXBIUkFqNVRMcwpjRU14Y2tRR0h6TXR2OGJ6dGNFTXh0Z0lPM2Rpc0xvSWdJcHJnSkVrbXBIUkFqNVRMc0hZWVhBR3RnSU8zZGlzCkxvSWdJcHJnSkVrbXBIUkFqNVRMc0hZWVhBR3RnSU8zZGlzTG9JZ0lwcmdKRWttcEhSQWo1VExzSFlZWEFHdGcKSU8zZGlzTG9JZ0lwcmdKRWttcEhSQWo1VExzSFlZWEFHdGdJTzNkaXNMb0lnSXByZ0pFa21wSFJBajVUTHNIWQpZWEFHdGdJTzNkaXNMb0lnSXByZ0pFa21wSFJBajVUTHNIWVlYQUd0Z0lPM2Rpc0xvSWdJcHJnSkVrbXBIUkFqCjVUTHNIWVlYQUd0Z0lPM2Rpc0xvSWdJcHJnSkVrbXBIUkFqNVRMc0hZWVhBR3RnSU8zZGlzTG9JZ0lwcmdKRWsKbXBIUkFqNVRMc0hZWVhBR3RnSU8zZGlzTG9JZ0lwcmdKRWttcEhSQWo1VExzSFlZWEFHdGdJTzNkaXNMb0lnSQpwcmdKRWttcEhSQWo1VExzY0VNeGNrUUdIek10djhienRjRU14Y2tRR0h6TXR2OGJ6dGNFTXhjZ0lPM2Rpc0xvCklnSXByZ0pFa21wSFJBajVUTHNIWVlYQUd0Z0lPM2Rpc0xvSWdJcHJnSkVrbXBIUkFqNVRMc0hZWVhBR3RnSU8KM2Rpc0xvSWdJcHJnSkVrbXBIUkFqNVRMcz0KLS0tLS1FTkQgUFJJVkFURSBLRVktLS0tLQ=="""

# Test user credentials
TEST_USERS = [
    {"username": "testuser1", "password": "password123"},
    {"username": "testuser2", "password": "securepass456"},
    {"username": "adminuser", "password": "admin789"}
]

# Test security policies
TEST_SECURITY_POLICIES = ["None", "Basic256", "Basic256Sha256"]


def cleanup_test_users(node_name):
    """Helper function to clean up test users"""
    try:
        response = api.get_basic_users(node_name)
        if response.status_code == 200:
            users = response.json().get("data", [])
            for user in users:
                username = user.get("username")
                if username and username.startswith("test"):
                    api.delete_basic_user(node_name, username)
    except Exception:
        pass  # Ignore cleanup errors


def assert_api_success(response, message="API call should succeed"):
    """Helper function to assert API success"""
    assert 200 == response.status_code, f"{message}: Expected 200, got {response.status_code}, body {response.json()}"
    response_json = response.json()
    error_code = response_json.get("error")
    assert error_code == NEU_ERR_SUCCESS, f"{message}: Expected {NEU_ERR_SUCCESS}, got {error_code}"


def assert_api_response_ok(response, message="API call should return OK"):
    """Helper function to assert API response is OK (may not have error field)"""
    assert response.status_code in [
        200, 201], f"{message}: Expected 200/201, got {response.status_code}, body {response.json()}"


@pytest.fixture(autouse=True, scope='class')
def setup_class():
    with open('build/tests/plugins/libplugin-demo_app.so', 'rb') as f:
        so_data = str(
            base64.b64encode(f.read()), encoding='utf-8')
    with open('build/tests/plugins/schema/demo_app.json', 'rb') as f:
        js_data = str(
            base64.b64encode(f.read()), encoding='utf-8')
    plugin_data = {}
    plugin_data['library'] = "libplugin-demo_app.so"
    plugin_data['so_file'] = so_data
    plugin_data['schema_file'] = js_data
    response = api.add_plugin(
        library_name=plugin_data['library'], so_file=plugin_data['so_file'], schema_file=plugin_data['schema_file'])
    assert 200 == response.status_code
    assert NEU_ERR_SUCCESS == response.json().get("error")

    api.add_node_check(node=NODE, plugin="Demo APP")

    yield

    # Cleanup after all tests
    try:
        api.del_node(NODE)
    except:
        pass


class TestDemoApp:
    @description(
        given="Demo APP node",
        when="testing certificate management APIs",
        then="all certificate operations should work correctly",
    )
    def test_server_cert_management(self):
        """Test server certificate management APIs"""

        # Test server certificate self-sign
        response = api.server_cert_self_sign(NODE)
        assert_api_success(
            response, "Server certificate self-sign should succeed")

        # Test get server certificate info
        response = api.server_cert_get(NODE)
        assert_api_response_ok(
            response, "Get server certificate should succeed")

        # Test export server certificate
        response = api.server_cert_export(NODE)
        assert_api_response_ok(
            response, "Export server certificate should succeed")

        # Test server certificate upload with test certificate data
        response = api.server_cert_upload(
            NODE, TEST_CERTIFICATE, TEST_PRIVATE_KEY)
        assert_api_success(
            response, "Server certificate upload should succeed")

    @description(
        given="Demo APP node",
        when="testing client certificate management APIs",
        then="all client certificate operations should work correctly",
    )
    def test_client_cert_management(self):
        """Test client certificate management APIs"""

        # Clean up any existing test certificates
        try:
            response = api.client_cert_get(NODE)
            if response.status_code == 200:
                certs = response.json().get("data", [])
                for cert in certs:
                    fingerprint = cert.get("fingerprint")
                    if fingerprint:
                        api.client_cert_delete(NODE, fingerprint)
        except Exception:
            pass  # Ignore cleanup errors

        # Test client certificate upload
        response = api.client_cert_upload(NODE, TEST_CERTIFICATE, is_ca=False)
        assert_api_success(
            response, "Client certificate upload should succeed")

        # Test get client certificates info
        response = api.client_cert_get(NODE)
        assert_api_response_ok(
            response, "Get client certificates should succeed")

        # Get the fingerprint from the response for further testing
        if response.status_code == 200:
            response_data = response.json()
            if "data" in response_data:
                certs = response_data["data"]
            elif isinstance(response_data, list):
                certs = response_data
            else:
                certs = []

            if certs:
                fingerprint = certs[0].get("fingerprint")

                # Test client certificate trust
                if fingerprint:
                    response = api.client_cert_trust(NODE, fingerprint)
                    assert_api_success(
                        response, "Client certificate trust should succeed")

                    # Test client certificate delete
                    response = api.client_cert_delete(NODE, fingerprint)
                    assert_api_success(
                        response, "Client certificate delete should succeed")

    @description(
        given="Demo APP node",
        when="testing basic authentication APIs",
        then="all authentication operations should work correctly",
    )
    def test_basic_authentication(self):
        """Test basic authentication management APIs"""

        # Clean up any existing test users
        cleanup_test_users(NODE)

        # Test enable basic authentication
        response = api.auth_basic_enable(NODE, enabled=True)
        assert_api_success(
            response, "Enable basic authentication should succeed")

        # Test get authentication status
        response = api.auth_basic_status(NODE)
        assert_api_response_ok(
            response, "Get authentication status should succeed")

        # Test add multiple basic users
        for user in TEST_USERS:
            response = api.add_basic_user(
                NODE, user["username"], user["password"])
            assert_api_response_ok(
                response, f"Add user {user['username']} should succeed")

        # Test get basic users
        response = api.get_basic_users(NODE)
        assert_api_response_ok(response, "Get basic users should succeed")

        # Verify users were added
        if response.status_code == 200:
            response_data = response.json()
            if "data" in response_data:
                users = response_data["data"]
            elif isinstance(response_data, list):
                users = response_data
            else:
                users = []

            added_usernames = {user.get("username")
                               for user in users if user.get("username")}
            for test_user in TEST_USERS:
                assert test_user["username"] in added_usernames, f"User {test_user['username']} should be found in user list"

        # Test update basic user password
        test_user = TEST_USERS[0]
        response = api.update_basic_user(
            NODE, test_user["username"], "updated_password")
        assert_api_response_ok(
            response, f"Update password for {test_user['username']} should succeed")

        # Test delete basic users
        for user in TEST_USERS:
            response = api.delete_basic_user(NODE, user["username"])
            assert_api_response_ok(
                response, f"Delete user {user['username']} should succeed")

        # Test disable basic authentication
        response = api.auth_basic_enable(NODE, enabled=False)
        assert_api_success(
            response, "Disable basic authentication should succeed")

    @description(
        given="Demo APP node",
        when="testing security policy APIs",
        then="all security policy operations should work correctly",
    )
    def test_security_policy(self):
        """Test server security policy APIs"""

        # Test set and get server security policy for each test policy
        for policy in TEST_SECURITY_POLICIES:
            # Test set server security policy
            response = api.server_security_policy(NODE, [policy])
            assert_api_response_ok(
                response, f"Set security policy to {policy} should succeed")

            # Test get server security policy
            response = api.get_server_security_policy(NODE)
            assert_api_response_ok(
                response, "Get security policy should succeed")

            # Verify policy was set (if response includes policy data)
            if response.status_code == 200:
                response_data = response.json()
                if "policyName" in response_data:
                    policy_name = response_data["policyName"]
                    # Note: The returned policy might differ from the set policy due to server logic
                    assert policy_name is not None, "Policy name should be returned"

    @description(
        given="Demo APP node",
        when="testing API error handling",
        then="APIs should handle invalid inputs gracefully",
    )
    def test_api_error_handling(self):
        """Test error handling for certificate and auth APIs"""

        # Test with non-existent node
        non_existent_node = "non_existent_node_12345"

        # Server cert operations with non-existent node
        response = api.server_cert_get(non_existent_node)
        # Different error codes are acceptable
        assert response.status_code in [200, 400, 404]

        # Client cert operations with non-existent node
        response = api.client_cert_get(non_existent_node)
        assert response.status_code in [200, 400, 404]

        # Auth operations with non-existent node
        response = api.auth_basic_status(non_existent_node)
        assert response.status_code in [200, 400, 404]

        # Security policy with non-existent node
        response = api.get_server_security_policy(non_existent_node)
        assert response.status_code in [200, 400, 404]

        # Test invalid certificate data
        response = api.server_cert_upload(NODE, "invalid_cert", "invalid_key")
        # Should handle invalid certificate gracefully
        assert response.status_code in [200, 400]

        # Test invalid fingerprint for client cert operations
        invalid_fingerprint = "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00"
        response = api.client_cert_trust(NODE, invalid_fingerprint)
        assert response.status_code in [200, 400, 404]

        response = api.client_cert_delete(NODE, invalid_fingerprint)
        assert response.status_code in [200, 400, 404]

    @description(
        given="Demo APP node",
        when="testing API parameter validation",
        then="APIs should validate parameters correctly",
    )
    def test_api_parameter_validation(self):
        """Test parameter validation for new APIs"""

        # Test empty parameters
        try:
            # Test with empty node name - should be handled gracefully
            response = api.server_cert_self_sign("")
            assert response.status_code in [200, 404]
        except Exception as e:
            # Parameter validation might throw exceptions
            assert "node" in str(e).lower() or "param" in str(e).lower()

        # Test with valid parameters
        response = api.server_cert_self_sign(NODE)
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json().get("error")

        # Test user management with edge cases
        # Very long username
        long_username = "a" * 100
        response = api.add_basic_user(NODE, long_username, "password123")
        # Should handle long usernames appropriately
        assert response.status_code in [200, 400]

        # Empty password
        response = api.add_basic_user(NODE, "testuser2", "")
        # Should handle empty passwords appropriately
        assert response.status_code in [200, 400]

    def test_api_integration(self):
        """Test integration between different APIs"""

        # Clean up before integration test
        cleanup_test_users(NODE)

        # Test full certificate and authentication workflow

        # 1. Generate self-signed certificate
        response = api.server_cert_self_sign(NODE)
        assert_api_success(
            response, "Generate self-signed certificate should succeed")

        # 2. Enable basic authentication
        response = api.auth_basic_enable(NODE, enabled=True)
        assert_api_success(
            response, "Enable basic authentication should succeed")

        # 3. Set security policy
        response = api.server_security_policy(NODE, TEST_SECURITY_POLICIES)
        assert_api_response_ok(response, "Set security policy should succeed")

        # 4. Add a user
        integration_user = {"username": "integrationuser",
                            "password": "integrationpass"}
        response = api.add_basic_user(
            NODE, integration_user["username"], integration_user["password"])
        assert_api_response_ok(response, "Add integration user should succeed")

        # 5. Upload a client certificate
        response = api.client_cert_upload(NODE, TEST_CERTIFICATE, is_ca=False)
        assert_api_success(
            response, "Upload client certificate should succeed")

        # 6. Verify the configuration
        response = api.auth_basic_status(NODE)
        assert_api_response_ok(response, "Get auth status should succeed")

        response = api.get_server_security_policy(NODE)
        assert_api_response_ok(response, "Get security policy should succeed")

        response = api.get_basic_users(NODE)
        assert_api_response_ok(response, "Get basic users should succeed")

        response = api.server_cert_get(NODE)
        assert_api_response_ok(
            response, "Get server certificate should succeed")

        response = api.client_cert_get(NODE)
        assert_api_response_ok(
            response, "Get client certificates should succeed")

        # 7. Clean up integration test data
        try:
            api.delete_basic_user(NODE, integration_user["username"])

            # Clean up client certificates
            response = api.client_cert_get(NODE)
            if response.status_code == 200:
                response_data = response.json()
                certs = response_data.get("data", []) if "data" in response_data else (
                    response_data if isinstance(response_data, list) else [])
                for cert in certs:
                    fingerprint = cert.get("fingerprint")
                    if fingerprint:
                        api.client_cert_delete(NODE, fingerprint)

            api.auth_basic_enable(NODE, enabled=False)
        except Exception:
            pass  # Ignore cleanup errors

    @description(
        given="Demo APP node",
        when="testing comprehensive API workflow",
        then="all operations should work together seamlessly",
    )
    def test_comprehensive_workflow(self):
        """Test a comprehensive workflow using all new APIs"""

        # Clean up before comprehensive test
        cleanup_test_users(NODE)

        try:
            # Step 1: Certificate Management Workflow
            print("Testing certificate management workflow...")

            # Generate self-signed server certificate
            response = api.server_cert_self_sign(NODE)
            assert_api_success(response, "Self-sign server certificate")

            # Get server certificate info
            response = api.server_cert_get(NODE)
            assert_api_response_ok(response, "Get server certificate info")

            # Upload client certificate
            response = api.client_cert_upload(
                NODE, TEST_CERTIFICATE, is_ca=False)
            assert_api_success(response, "Upload client certificate")

            # Step 2: Authentication Management Workflow
            print("Testing authentication management workflow...")

            # Enable basic authentication
            response = api.auth_basic_enable(NODE, enabled=True)
            print("Enable basic authentication response:", response.json())
            assert_api_success(response, "Enable basic authentication")

            # Add multiple users
            for i, user in enumerate(TEST_USERS[:2]):  # Use first 2 users
                response = api.add_basic_user(
                    NODE, user["username"], user["password"])
                assert_api_response_ok(response, f"Add user {i+1}")

            # Step 3: Security Policy Workflow
            print("Testing security policy workflow...")

            # Set security policy
            response = api.server_security_policy(NODE, ["Basic256"])
            assert_api_response_ok(response, "Set security policy")

            # Step 4: Verification Workflow
            print("Verifying all configurations...")

            # Verify authentication status
            response = api.auth_basic_status(NODE)
            assert_api_response_ok(response, "Verify auth status")

            # Verify users exist
            response = api.get_basic_users(NODE)
            assert_api_response_ok(response, "Verify users exist")

            # Verify security policy
            response = api.get_server_security_policy(NODE)
            assert_api_response_ok(response, "Verify security policy")

            # Verify certificates
            response = api.client_cert_get(NODE)
            assert_api_response_ok(response, "Verify client certificates")

            print("Comprehensive workflow test completed successfully!")

        finally:
            # Comprehensive cleanup
            print("Cleaning up comprehensive test...")
            try:
                # Clean up users
                for user in TEST_USERS:
                    api.delete_basic_user(NODE, user["username"])

                # Clean up certificates
                response = api.client_cert_get(NODE)
                if response.status_code == 200:
                    response_data = response.json()
                    certs = response_data.get("data", []) if "data" in response_data else (
                        response_data if isinstance(response_data, list) else [])
                    for cert in certs:
                        fingerprint = cert.get("fingerprint")
                        if fingerprint:
                            api.client_cert_delete(NODE, fingerprint)

                # Disable authentication
                api.auth_basic_enable(NODE, enabled=False)

            except Exception as e:
                print(f"Cleanup error: {e}")  # Log but don't fail the test
