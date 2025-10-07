#!/usr/bin/env python3
"""
Comprehensive MQTT Testing Script for ESP32 Word Clock
This script provides all the MQTT testing capabilities you need.
"""

import paho.mqtt.client as mqtt
import json
import time
import sys
import argparse
import threading
from datetime import datetime

class WordClockMQTTTester:
    def __init__(self, broker, port, username, password, use_tls=True):
        self.broker = broker
        self.port = port
        self.username = username
        self.password = password
        self.use_tls = use_tls
        self.client = None
        self.connected = False
        
    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            print(f"✅ Connected to MQTT broker {self.broker}:{self.port}")
            self.connected = True
            # Subscribe to all wordclock topics
            client.subscribe("wordclock/+")
            client.subscribe("home/esp32_core/+")
            print("📡 Subscribed to wordclock/+ and home/esp32_core/+")
        else:
            print(f"❌ Failed to connect, return code {rc}")
            
    def on_message(self, client, userdata, msg):
        timestamp = datetime.now().strftime("%H:%M:%S")
        try:
            # Try to parse as JSON for pretty printing
            payload = json.loads(msg.payload.decode())
            print(f"[{timestamp}] 📨 {msg.topic}")
            print(f"           {json.dumps(payload, indent=2)}")
        except:
            # Plain text message
            print(f"[{timestamp}] 📨 {msg.topic}: {msg.payload.decode()}")
        print()
            
    def on_disconnect(self, client, userdata, rc):
        print(f"🔌 Disconnected from MQTT broker")
        self.connected = False
        
    def connect(self):
        """Connect to MQTT broker"""
        self.client = mqtt.Client()
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.on_disconnect = self.on_disconnect
        
        if self.username and self.password:
            self.client.username_pw_set(self.username, self.password)
            
        if self.use_tls:
            self.client.tls_set()
            
        try:
            self.client.connect(self.broker, self.port, 60)
            self.client.loop_start()
            
            # Wait for connection
            timeout = 10
            while not self.connected and timeout > 0:
                time.sleep(0.1)
                timeout -= 0.1
                
            if not self.connected:
                print("❌ Connection timeout")
                return False
                
            return True
        except Exception as e:
            print(f"❌ Connection error: {e}")
            return False
            
    def disconnect(self):
        """Disconnect from MQTT broker"""
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()
            
    def send_command(self, command, parameters=None):
        """Send a command to the wordclock"""
        if not self.connected:
            print("❌ Not connected to MQTT broker")
            return False
            
        if parameters:
            message = {"command": command, "parameters": parameters}
        else:
            message = {"command": command}
            
        try:
            topic = "wordclock/command"
            payload = json.dumps(message)
            result = self.client.publish(topic, payload)
            print(f"📤 Sent to {topic}: {payload}")
            return result.rc == mqtt.MQTT_ERR_SUCCESS
        except Exception as e:
            print(f"❌ Error sending command: {e}")
            return False
            
    def send_simple_command(self, command_str):
        """Send a simple command string (legacy format)"""
        if not self.connected:
            print("❌ Not connected to MQTT broker")
            return False
            
        try:
            topic = "wordclock/command"
            result = self.client.publish(topic, command_str)
            print(f"📤 Sent to {topic}: {command_str}")
            return result.rc == mqtt.MQTT_ERR_SUCCESS
        except Exception as e:
            print(f"❌ Error sending command: {e}")
            return False

def run_interactive_mode(tester):
    """Run in interactive command mode"""
    print("\n🎮 Interactive Mode - Enter commands (type 'help' for options, 'quit' to exit)")
    print("="*60)
    
    while True:
        try:
            command = input("\nwordclock> ").strip()
            
            if command.lower() in ['quit', 'exit', 'q']:
                break
            elif command.lower() == 'help':
                print_help()
            elif command == '':
                continue
            elif command.startswith('{'):
                # JSON command
                try:
                    json.loads(command)  # Validate JSON
                    result = tester.client.publish("wordclock/command", command)
                    print(f"📤 Sent JSON: {command}")
                except json.JSONDecodeError:
                    print("❌ Invalid JSON format")
            else:
                # Simple command
                tester.send_simple_command(command)
                
        except KeyboardInterrupt:
            break
        except EOFError:
            break
            
    print("\n👋 Exiting interactive mode")

def print_help():
    """Print help information"""
    print("""
📖 Available Commands:

Simple Commands (legacy format):
  status                     - Get device status
  restart                    - Restart the device  
  reset_wifi                 - Clear WiFi credentials
  test_transitions_start     - Start transition testing
  test_transitions_stop      - Stop transition testing
  set_time 14:30            - Set time for testing

JSON Commands (Tier 1 format):
  {"command": "status"}
  {"command": "restart"}
  {"command": "set_brightness", "parameters": {"individual": 50, "global": 180}}
  {"command": "set_time", "parameters": {"time": "14:30"}}
  
Special Commands:
  help                       - Show this help
  quit/exit/q               - Exit interactive mode
  
You can also paste JSON commands directly!
    """)

def run_automated_tests(tester):
    """Run automated test suite"""
    print("\n🧪 Running Automated Test Suite")
    print("="*40)
    
    tests = [
        ("Device Status", "status"),
        ("Transition Test Start", "test_transitions_start"),
        ("Time Set Test", "set_time 09:30"),
        ("Time Set Test 2", "set_time 14:45"),
        ("Transition Test Stop", "test_transitions_stop"),
        ("Final Status Check", "status"),
    ]
    
    for test_name, command in tests:
        print(f"\n🔬 Test: {test_name}")
        tester.send_simple_command(command)
        time.sleep(3)  # Wait between tests
        
    print("\n✅ Automated tests completed!")

def monitor_mode(tester):
    """Run in monitoring mode"""
    print("\n👀 Monitoring Mode - Listening for all messages...")
    print("Press Ctrl+C to stop")
    print("="*50)
    
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n🛑 Monitoring stopped")

def main():
    parser = argparse.ArgumentParser(description='ESP32 Word Clock MQTT Tester')
    parser.add_argument('--broker', default='your-hivemq-broker.hivemq.cloud', 
                       help='MQTT broker hostname')
    parser.add_argument('--port', type=int, default=8883, 
                       help='MQTT broker port')
    parser.add_argument('--username', help='MQTT username')
    parser.add_argument('--password', help='MQTT password')
    parser.add_argument('--no-tls', action='store_true', 
                       help='Disable TLS (use for testing only)')
    parser.add_argument('--command', help='Send single command and exit')
    parser.add_argument('--test', action='store_true', 
                       help='Run automated test suite')
    parser.add_argument('--monitor', action='store_true', 
                       help='Monitor mode (listen only)')
    
    args = parser.parse_args()
    
    # Create tester instance
    tester = WordClockMQTTTester(
        broker=args.broker,
        port=args.port,
        username=args.username,
        password=args.password,
        use_tls=not args.no_tls
    )
    
    # Connect to broker
    if not tester.connect():
        sys.exit(1)
        
    try:
        if args.command:
            # Single command mode
            tester.send_simple_command(args.command)
            time.sleep(2)  # Wait for response
        elif args.test:
            # Automated test mode
            run_automated_tests(tester)
        elif args.monitor:
            # Monitor mode
            monitor_mode(tester)
        else:
            # Interactive mode (default)
            run_interactive_mode(tester)
            
    finally:
        tester.disconnect()

if __name__ == "__main__":
    main()