#!/usr/bin/env python3
"""
Analyze ESP32 Word Clock test logs and generate detailed reports
"""

import json
import re
import sys
import os
from datetime import datetime
from collections import defaultdict

class LogAnalyzer:
    def __init__(self, logs_dir):
        self.logs_dir = logs_dir
        self.serial_log = os.path.join(logs_dir, "serial_monitor.log")
        self.mqtt_log = os.path.join(logs_dir, "mqtt_messages.log")
        self.combined_log = os.path.join(logs_dir, "combined_output.log")
        
    def analyze_serial_log(self):
        """Analyze ESP32 serial output"""
        if not os.path.exists(self.serial_log):
            return {"error": "Serial log not found"}
            
        results = {
            "total_lines": 0,
            "errors": [],
            "warnings": [],
            "info_messages": [],
            "esp_logs": defaultdict(list),
            "reboots": [],
            "timestamps": []
        }
        
        with open(self.serial_log, 'r') as f:
            for line in f:
                results["total_lines"] += 1
                
                # Check for ESP-IDF log levels
                if match := re.search(r'([EWID]) \((\d+)\) (\w+): (.+)', line):
                    level, timestamp, tag, message = match.groups()
                    entry = {
                        "timestamp": timestamp,
                        "tag": tag,
                        "message": message.strip()
                    }
                    
                    if level == 'E':
                        results["errors"].append(entry)
                    elif level == 'W':
                        results["warnings"].append(entry)
                    elif level == 'I':
                        results["info_messages"].append(entry)
                    
                    results["esp_logs"][tag].append(entry)
                
                # Check for reboots
                if "ESP-ROM:esp32" in line or "Rebooting..." in line:
                    results["reboots"].append(line.strip())
                    
        return results
    
    def analyze_mqtt_log(self):
        """Analyze MQTT messages"""
        if not os.path.exists(self.mqtt_log):
            return {"error": "MQTT log not found"}
            
        results = {
            "total_messages": 0,
            "topics": defaultdict(list),
            "commands_sent": [],
            "responses_received": [],
            "home_assistant_configs": 0,
            "status_messages": []
        }
        
        with open(self.mqtt_log, 'r') as f:
            for line in f:
                if "[MQTT]" in line:
                    results["total_messages"] += 1
                    
                    # Parse topic and payload
                    if match := re.search(r'\[MQTT\] ([^\s]+) (.+)', line):
                        topic, payload = match.groups()
                        
                        entry = {
                            "topic": topic,
                            "payload": payload.strip(),
                            "timestamp": datetime.now().isoformat()
                        }
                        
                        results["topics"][topic].append(entry)
                        
                        # Categorize messages
                        if "/command" in topic:
                            results["commands_sent"].append(entry)
                        elif "/status" in topic:
                            results["status_messages"].append(entry)
                        elif "homeassistant/" in topic:
                            results["home_assistant_configs"] += 1
                            
        return results
    
    def correlate_command_responses(self):
        """Correlate commands with their responses"""
        mqtt_data = self.analyze_mqtt_log()
        serial_data = self.analyze_serial_log()
        
        correlations = []
        
        for cmd in mqtt_data.get("commands_sent", []):
            correlation = {
                "command": cmd["payload"],
                "topic": cmd["topic"],
                "mqtt_responses": [],
                "serial_responses": []
            }
            
            # Look for related MQTT responses
            for status in mqtt_data.get("status_messages", []):
                if cmd["payload"].lower() in status["payload"].lower():
                    correlation["mqtt_responses"].append(status)
            
            # Look for related serial logs
            cmd_keyword = cmd["payload"].split()[0].lower()
            for tag, logs in serial_data.get("esp_logs", {}).items():
                for log in logs:
                    if cmd_keyword in log["message"].lower():
                        correlation["serial_responses"].append({
                            "tag": tag,
                            "message": log["message"]
                        })
            
            correlations.append(correlation)
            
        return correlations
    
    def generate_report(self):
        """Generate comprehensive analysis report"""
        print("📊 ESP32 Word Clock Log Analysis Report")
        print("=====================================")
        print()
        
        # Serial log analysis
        serial_data = self.analyze_serial_log()
        print("📟 Serial Monitor Analysis:")
        print(f"   Total lines: {serial_data.get('total_lines', 0)}")
        print(f"   Errors: {len(serial_data.get('errors', []))}")
        print(f"   Warnings: {len(serial_data.get('warnings', []))}")
        print(f"   Reboots: {len(serial_data.get('reboots', []))}")
        print()
        
        if serial_data.get('errors'):
            print("   ❌ Recent Errors:")
            for err in serial_data['errors'][-5:]:
                print(f"      [{err['tag']}] {err['message']}")
            print()
        
        # MQTT log analysis
        mqtt_data = self.analyze_mqtt_log()
        print("🌐 MQTT Message Analysis:")
        print(f"   Total messages: {mqtt_data.get('total_messages', 0)}")
        print(f"   Unique topics: {len(mqtt_data.get('topics', {}))}")
        print(f"   Commands sent: {len(mqtt_data.get('commands_sent', []))}")
        print(f"   Status messages: {len(mqtt_data.get('status_messages', []))}")
        print(f"   HA configs: {mqtt_data.get('home_assistant_configs', 0)}")
        print()
        
        # Command/Response correlation
        correlations = self.correlate_command_responses()
        print("🔗 Command/Response Correlations:")
        for corr in correlations:
            print(f"   Command: {corr['command']}")
            print(f"   MQTT responses: {len(corr['mqtt_responses'])}")
            print(f"   Serial responses: {len(corr['serial_responses'])}")
            print()
        
        # Save detailed JSON report
        report_file = os.path.join(self.logs_dir, f"analysis_report_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json")
        with open(report_file, 'w') as f:
            json.dump({
                "serial_analysis": serial_data,
                "mqtt_analysis": mqtt_data,
                "correlations": correlations,
                "timestamp": datetime.now().isoformat()
            }, f, indent=2)
        
        print(f"📁 Detailed report saved: {report_file}")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        logs_dir = sys.argv[1]
    else:
        logs_dir = os.path.join(os.path.dirname(__file__), "../logs")
    
    analyzer = LogAnalyzer(logs_dir)
    analyzer.generate_report()