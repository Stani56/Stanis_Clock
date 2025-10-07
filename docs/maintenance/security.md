# Security Policy

## Supported Versions

We provide security updates for the following versions of the ESP32 German Word Clock:

| Version | Supported          |
| ------- | ------------------ |
| 2.0.x   | :white_check_mark: |
| 1.0.x   | :white_check_mark: |
| < 1.0   | :x:                |

## Security Considerations

### Network Security Features

This project implements several security measures:

✅ **MQTT TLS Encryption**: All MQTT communication uses TLS 1.2+ encryption  
✅ **Certificate Validation**: ESP32 validates HiveMQ Cloud certificates against built-in CA bundle  
✅ **No Plain Text Credentials**: WiFi and MQTT credentials stored securely in NVS flash  
✅ **AP Mode Security**: Configuration portal uses WPA2 with password protection  
✅ **Input Validation**: MQTT commands are validated before execution  

### Known Security Limitations

⚠️ **Local Network Access**: Device is accessible via local network when connected  
⚠️ **Physical Access**: Reset button can clear WiFi credentials (by design)  
⚠️ **Web Portal**: Configuration portal is unencrypted HTTP (local access only)  
⚠️ **Debug Logs**: May contain sensitive information when debug mode enabled  

## Reporting Security Vulnerabilities

If you discover a security vulnerability, please follow responsible disclosure:

### 🔒 Private Reporting (Preferred)

1. **Do NOT** create a public GitHub issue for security vulnerabilities
2. **Email** details to: [your-email@example.com] (replace with actual contact)
3. **Include** the following information:
   - Description of the vulnerability
   - Steps to reproduce the issue
   - Potential impact assessment
   - Suggested fix (if you have one)

### 📧 What to Include

```
Subject: [SECURITY] ESP32 Word Clock Vulnerability Report

**Vulnerability Type:** [e.g., Network, Authentication, Input Validation]
**Severity:** [Critical/High/Medium/Low]
**Component:** [e.g., MQTT Manager, WiFi Manager, Web Server]

**Description:**
[Detailed description of the vulnerability]

**Reproduction Steps:**
1. [Step 1]
2. [Step 2]
3. [Result]

**Impact:**
[What could an attacker do with this vulnerability?]

**Environment:**
- ESP-IDF Version: [version]
- Project Version: [version]
- Hardware Setup: [brief description]

**Additional Information:**
[Any other relevant details]
```

## Response Timeline

We aim to respond to security reports within:

- **Initial Response**: 48 hours
- **Triage Assessment**: 7 days  
- **Fix Development**: 30 days (depending on complexity)
- **Public Disclosure**: After fix is available and deployed

## Security Best Practices for Users

### Network Security

🔐 **Use Strong WiFi Passwords**: Ensure your WiFi network uses WPA2/WPA3 with strong passwords  
🔐 **Secure MQTT Broker**: Use TLS-enabled MQTT brokers with authentication  
🔐 **Network Isolation**: Consider placing IoT devices on isolated network segments  
🔐 **Regular Updates**: Keep ESP-IDF and project code updated to latest versions  

### Physical Security

🔒 **Secure Physical Access**: Protect physical access to the ESP32 and reset button  
🔒 **Console Access**: Disable debug logging in production deployments  
🔒 **Flash Encryption**: Consider enabling ESP32 flash encryption for sensitive deployments  

### Configuration Security

⚙️ **Change Default Passwords**: Modify any default credentials in the code  
⚙️ **Minimal Privileges**: Configure MQTT users with minimal required permissions  
⚙️ **Monitor Logs**: Watch for unusual connection attempts or commands  
⚙️ **Regular Audits**: Periodically review device configuration and access logs  

## Security-Related Configuration

### Secure Boot (Advanced)

For high-security deployments, consider enabling ESP32 secure boot:

```bash
# Enable secure boot in ESP-IDF configuration
idf.py menuconfig
# Navigate to: Security features -> Enable hardware secure boot in bootloader
```

### Flash Encryption (Advanced)

For data protection at rest:

```bash
# Enable flash encryption in ESP-IDF configuration  
idf.py menuconfig
# Navigate to: Security features -> Enable flash encryption on boot
```

⚠️ **Warning**: These features are irreversible and require careful planning.

## Vulnerability Disclosure Policy

### Coordinated Disclosure

We follow responsible disclosure practices:

1. **Report Received**: We acknowledge receipt and begin investigation
2. **Vulnerability Confirmed**: We confirm the issue and assess impact
3. **Fix Developed**: We develop and test a security fix
4. **Release Prepared**: We prepare a security release with appropriate versioning
5. **Public Disclosure**: We publicly disclose the vulnerability after fix is available

### Public Recognition

Security researchers who responsibly report vulnerabilities will be:

- Credited in release notes (if desired)
- Listed in project acknowledgments
- Notified when fixes are available

## Security Updates

Security fixes are distributed via:

- **GitHub Releases**: Tagged releases with security patch notes
- **GitHub Security Advisories**: For high-severity vulnerabilities
- **Documentation Updates**: Security best practices and configuration guidance

## Contact Information

For security-related inquiries:

- **Security Reports**: [your-security-email@example.com]
- **General Questions**: Use GitHub Discussions with [security] tag
- **Project Maintainer**: [@stani56](https://github.com/stani56)

---

**Note**: This security policy applies to the ESP32 German Word Clock project code and documentation. For security issues in ESP-IDF framework, components, or third-party libraries, please report to the respective maintainers.

**Last Updated**: 2025-07-29