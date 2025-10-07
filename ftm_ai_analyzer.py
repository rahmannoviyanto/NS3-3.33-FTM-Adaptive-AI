#!/usr/bin/env python3
"""
FTM AI Analyzer and Visualization
Analyzes NS-3 simulation results and provides AI-driven recommendations
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os
import sys
from datetime import datetime

class FTMAnalyzer:
    def __init__(self, csv_path="result/ftm_metrics.csv"):
        """Initialize analyzer with CSV data"""
        if not os.path.exists(csv_path):
            print(f"Error: {csv_path} not found!")
            print("Please run the NS-3 simulation first.")
            sys.exit(1)
        
        self.df = pd.read_csv(csv_path)
        self.output_dir = "result"
        
        # Separate data by flow
        self.sta1_data = self.df[self.df['Flow'] == 'AP1-STA1'].copy()
        self.sta2_data = self.df[self.df['Flow'] == 'AP2-STA2'].copy()
        
        print(f"Loaded {len(self.df)} data points from simulation")
        print(f"STA1 (Static): {len(self.sta1_data)} samples")
        print(f"STA2 (Mobile): {len(self.sta2_data)} samples")
    
    def analyze_performance(self):
        """Analyze performance metrics"""
        print("\n" + "="*70)
        print("FTM ADAPTIVE WIFI - AI PERFORMANCE ANALYSIS")
        print("="*70)
        
        # STA1 Analysis (Static)
        print("\n[STA1 - STATIC STATION (5m from AP1)]")
        print("-" * 70)
        if len(self.sta1_data) > 0:
            print(f"Average Distance:    {self.sta1_data['Distance(m)'].mean():.2f} m")
            print(f"Average Throughput:  {self.sta1_data['Throughput(Mbps)'].mean():.3f} Mbps")
            print(f"Average PDR:         {self.sta1_data['PDR(%)'].mean():.2f}%")
            print(f"Average Loss:        {self.sta1_data['Loss(%)'].mean():.2f}%")
            print(f"Average Delay:       {self.sta1_data['Delay(ms)'].mean():.3f} ms")
            print(f"Average RSSI:        {self.sta1_data['RSSI(dBm)'].mean():.2f} dBm")
            print(f"Throughput Range:    {self.sta1_data['Throughput(Mbps)'].min():.3f} - "
                  f"{self.sta1_data['Throughput(Mbps)'].max():.3f} Mbps")
        
        # STA2 Analysis (Mobile)
        print("\n[STA2 - MOBILE STATION (5m -> 20m -> 10m from AP2)]")
        print("-" * 70)
        if len(self.sta2_data) > 0:
            print(f"Distance Range:      {self.sta2_data['Distance(m)'].min():.2f} - "
                  f"{self.sta2_data['Distance(m)'].max():.2f} m")
            print(f"Average Throughput:  {self.sta2_data['Throughput(Mbps)'].mean():.3f} Mbps")
            print(f"Average PDR:         {self.sta2_data['PDR(%)'].mean():.2f}%")
            print(f"Average Loss:        {self.sta2_data['Loss(%)'].mean():.2f}%")
            print(f"Average Delay:       {self.sta2_data['Delay(ms)'].mean():.3f} ms")
            print(f"Average RSSI:        {self.sta2_data['RSSI(dBm)'].mean():.2f} dBm")
            print(f"Throughput Range:    {self.sta2_data['Throughput(Mbps)'].min():.3f} - "
                  f"{self.sta2_data['Throughput(Mbps)'].max():.3f} Mbps")
            
            # Movement phases
            phase1 = self.sta2_data[self.sta2_data['Time(s)'] <= 5]
            phase2 = self.sta2_data[(self.sta2_data['Time(s)'] > 5) & 
                                   (self.sta2_data['Time(s)'] <= 10)]
            phase3 = self.sta2_data[(self.sta2_data['Time(s)'] > 10) & 
                                   (self.sta2_data['Time(s)'] <= 15)]
            phase4 = self.sta2_data[self.sta2_data['Time(s)'] > 15]
            
            print("\nMovement Phase Analysis:")
            print(f"  Phase 1 (0-5s, ~5m):    Avg Throughput = "
                  f"{phase1['Throughput(Mbps)'].mean():.3f} Mbps")
            print(f"  Phase 2 (5-10s, moving): Avg Throughput = "
                  f"{phase2['Throughput(Mbps)'].mean():.3f} Mbps")
            print(f"  Phase 3 (10-15s, ~20m):  Avg Throughput = "
                  f"{phase3['Throughput(Mbps)'].mean():.3f} Mbps")
            print(f"  Phase 4 (15-20s, moving): Avg Throughput = "
                  f"{phase4['Throughput(Mbps)'].mean():.3f} Mbps")
    
    def ai_recommendations(self):
        """Generate AI-driven recommendations"""
        print("\n" + "="*70)
        print("AI RECOMMENDATIONS")
        print("="*70)
        
        recommendations = []
        
        # Analyze STA2 performance degradation
        if len(self.sta2_data) > 0:
            far_distance = self.sta2_data[self.sta2_data['Distance(m)'] > 15]
            if len(far_distance) > 0:
                avg_throughput_far = far_distance['Throughput(Mbps)'].mean()
                avg_rssi_far = far_distance['RSSI(dBm)'].mean()
                
                if avg_throughput_far < 3.0:  # Less than 60% of target
                    recommendations.append({
                        'priority': 'HIGH',
                        'issue': 'Severe throughput degradation at far distances',
                        'metric': f'Average throughput: {avg_throughput_far:.3f} Mbps',
                        'action': 'Increase TX power by 3-4 dBm or implement beamforming'
                    })
                
                if avg_rssi_far < -75:
                    recommendations.append({
                        'priority': 'MEDIUM',
                        'issue': 'Weak signal strength at far distances',
                        'metric': f'Average RSSI: {avg_rssi_far:.2f} dBm',
                        'action': 'Deploy additional AP or use directional antennas'
                    })
            
            # Check packet loss
            high_loss = self.sta2_data[self.sta2_data['Loss(%)'] > 10]
            if len(high_loss) > 0:
                recommendations.append({
                    'priority': 'MEDIUM',
                    'issue': 'High packet loss detected',
                    'metric': f'{len(high_loss)} intervals with >10% loss',
                    'action': 'Implement rate adaptation or increase retransmission limit'
                })
        
        # Analyze STA1 stability
        if len(self.sta1_data) > 0:
            throughput_std = self.sta1_data['Throughput(Mbps)'].std()
            if throughput_std > 0.5:
                recommendations.append({
                    'priority': 'LOW',
                    'issue': 'Throughput variability in static station',
                    'metric': f'Std dev: {throughput_std:.3f} Mbps',
                    'action': 'Check for interference or adjust channel selection'
                })
        
        # Print recommendations
        if recommendations:
            for i, rec in enumerate(recommendations, 1):
                print(f"\n{i}. [{rec['priority']}] {rec['issue']}")
                print(f"   Metric: {rec['metric']}")
                print(f"   Recommended Action: {rec['action']}")
        else:
            print("\n✓ No critical issues detected. System performing optimally.")
        
        return recommendations
    
    def visualize_results(self):
        """Create comprehensive visualization"""
        print("\n" + "="*70)
        print("GENERATING VISUALIZATIONS")
        print("="*70)
        
        fig, axes = plt.subplots(3, 2, figsize=(16, 12))
        fig.suptitle('FTM Adaptive WiFi Performance Analysis', 
                     fontsize=16, fontweight='bold')
        
        # Plot 1: Distance vs Time
        ax1 = axes[0, 0]
        if len(self.sta1_data) > 0:
            ax1.plot(self.sta1_data['Time(s)'], self.sta1_data['Distance(m)'], 
                    'b-o', label='STA1 (Static)', linewidth=2, markersize=4)
        if len(self.sta2_data) > 0:
            ax1.plot(self.sta2_data['Time(s)'], self.sta2_data['Distance(m)'], 
                    'r-s', label='STA2 (Mobile)', linewidth=2, markersize=4)
        ax1.set_xlabel('Time (s)', fontweight='bold')
        ax1.set_ylabel('Distance (m)', fontweight='bold')
        ax1.set_title('Distance from AP over Time')
        ax1.legend()
        ax1.grid(True, alpha=0.3)
        
        # Plot 2: Throughput vs Time
        ax2 = axes[0, 1]
        if len(self.sta1_data) > 0:
            ax2.plot(self.sta1_data['Time(s)'], self.sta1_data['Throughput(Mbps)'], 
                    'b-o', label='STA1 (Static)', linewidth=2, markersize=4)
        if len(self.sta2_data) > 0:
            ax2.plot(self.sta2_data['Time(s)'], self.sta2_data['Throughput(Mbps)'], 
                    'r-s', label='STA2 (Mobile)', linewidth=2, markersize=4)
        ax2.axhline(y=5.0, color='g', linestyle='--', label='Target (5 Mbps)', alpha=0.7)
        ax2.set_xlabel('Time (s)', fontweight='bold')
        ax2.set_ylabel('Throughput (Mbps)', fontweight='bold')
        ax2.set_title('Throughput over Time')
        ax2.legend()
        ax2.grid(True, alpha=0.3)
        
        # Plot 3: Distance vs Throughput (Correlation)
        ax3 = axes[1, 0]
        if len(self.sta2_data) > 0:
            ax3.scatter(self.sta2_data['Distance(m)'], 
                       self.sta2_data['Throughput(Mbps)'], 
                       c=self.sta2_data['Time(s)'], cmap='viridis', 
                       s=100, alpha=0.6, edgecolors='black')
            cbar = plt.colorbar(ax3.collections[0], ax=ax3)
            cbar.set_label('Time (s)', fontweight='bold')
        ax3.set_xlabel('Distance (m)', fontweight='bold')
        ax3.set_ylabel('Throughput (Mbps)', fontweight='bold')
        ax3.set_title('Distance vs Throughput (STA2)')
        ax3.grid(True, alpha=0.3)
        
        # Plot 4: RSSI vs Time
        ax4 = axes[1, 1]
        if len(self.sta1_data) > 0:
            ax4.plot(self.sta1_data['Time(s)'], self.sta1_data['RSSI(dBm)'], 
                    'b-o', label='STA1 (Static)', linewidth=2, markersize=4)
        if len(self.sta2_data) > 0:
            ax4.plot(self.sta2_data['Time(s)'], self.sta2_data['RSSI(dBm)'], 
                    'r-s', label='STA2 (Mobile)', linewidth=2, markersize=4)
        ax4.axhline(y=-70, color='orange', linestyle='--', 
                   label='Weak Signal (-70 dBm)', alpha=0.7)
        ax4.set_xlabel('Time (s)', fontweight='bold')
        ax4.set_ylabel('RSSI (dBm)', fontweight='bold')
        ax4.set_title('Received Signal Strength over Time')
        ax4.legend()
        ax4.grid(True, alpha=0.3)
        
        # Plot 5: PDR and Loss
        ax5 = axes[2, 0]
        if len(self.sta1_data) > 0:
            ax5.plot(self.sta1_data['Time(s)'], self.sta1_data['PDR(%)'], 
                    'b-o', label='STA1 PDR', linewidth=2, markersize=4)
        if len(self.sta2_data) > 0:
            ax5.plot(self.sta2_data['Time(s)'], self.sta2_data['PDR(%)'], 
                    'r-s', label='STA2 PDR', linewidth=2, markersize=4)
        ax5.axhline(y=90, color='g', linestyle='--', 
                   label='Target (90%)', alpha=0.7)
        ax5.set_xlabel('Time (s)', fontweight='bold')
        ax5.set_ylabel('Packet Delivery Ratio (%)', fontweight='bold')
        ax5.set_title('PDR over Time')
        ax5.legend()
        ax5.grid(True, alpha=0.3)
        
        # Plot 6: TX Power Adjustment
        ax6 = axes[2, 1]
        if len(self.sta1_data) > 0:
            ax6.plot(self.sta1_data['Time(s)'], self.sta1_data['TxPower(dBm)'], 
                    'b-o', label='AP1 TX Power', linewidth=2, markersize=4)
        if len(self.sta2_data) > 0:
            ax6.plot(self.sta2_data['Time(s)'], self.sta2_data['TxPower(dBm)'], 
                    'r-s', label='AP2 TX Power', linewidth=2, markersize=4)
        ax6.set_xlabel('Time (s)', fontweight='bold')
        ax6.set_ylabel('TX Power (dBm)', fontweight='bold')
        ax6.set_title('AI-Adaptive TX Power Adjustment')
        ax6.legend()
        ax6.grid(True, alpha=0.3)
        
        plt.tight_layout()
        output_path = os.path.join(self.output_dir, 'ftm_analysis.png')
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        print(f"✓ Saved visualization: {output_path}")
        plt.close()
        
        # Create AI decision timeline
        self.plot_ai_decisions()
    
    def plot_ai_decisions(self):
        """Plot AI decision timeline"""
        fig, ax = plt.subplots(figsize=(14, 6))
        
        # Count AI decisions
        if len(self.sta2_data) > 0:
            decision_counts = self.sta2_data['AI_Decision'].value_counts()
            
            # Timeline of decisions
            decisions = self.sta2_data['AI_Decision'].tolist()
            times = self.sta2_data['Time(s)'].tolist()
            
            # Color map for decisions
            color_map = {
                'maintain': 'green',
                'increase_power': 'orange',
                'increase_power_and_change_channel': 'red',
                'decrease_power': 'blue'
            }
            
            colors = [color_map.get(d, 'gray') for d in decisions]
            
            ax.scatter(times, [1]*len(times), c=colors, s=200, 
                      alpha=0.7, edgecolors='black', linewidths=1.5)
            
            # Add distance as background
            ax2 = ax.twinx()
            ax2.plot(self.sta2_data['Time(s)'], self.sta2_data['Distance(m)'], 
                    'k--', alpha=0.3, linewidth=2, label='Distance')
            ax2.set_ylabel('Distance (m)', fontweight='bold')
            
            # Legend
            from matplotlib.patches import Patch
            legend_elements = [Patch(facecolor=color, label=decision) 
                             for decision, color in color_map.items() 
                             if decision in decisions]
            ax.legend(handles=legend_elements, loc='upper left')
            
            ax.set_xlabel('Time (s)', fontweight='bold')
            ax.set_ylabel('AI Decision Event', fontweight='bold')
            ax.set_title('AI Decision Timeline for STA2 Adaptive Control', 
                        fontweight='bold')
            ax.set_ylim(0.5, 1.5)
            ax.set_yticks([])
            ax.grid(True, alpha=0.3, axis='x')
            
            plt.tight_layout()
            output_path = os.path.join(self.output_dir, 'ai_decision_timeline.png')
            plt.savefig(output_path, dpi=300, bbox_inches='tight')
            print(f"✓ Saved AI decision timeline: {output_path}")
            plt.close()
    
    def export_summary_report(self):
        """Export comprehensive summary report"""
        report_path = os.path.join(self.output_dir, 'ftm_summary_report.txt')
        
        with open(report_path, 'w') as f:
            f.write("="*70 + "\n")
            f.write("FTM ADAPTIVE WIFI - COMPREHENSIVE ANALYSIS REPORT\n")
            f.write("="*70 + "\n")
            f.write(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"Total Samples: {len(self.df)}\n\n")
            
            # STA1 Summary
            f.write("[STA1 - STATIC STATION]\n")
            f.write("-"*70 + "\n")
            if len(self.sta1_data) > 0:
                f.write(f"Average Distance:    {self.sta1_data['Distance(m)'].mean():.2f} m\n")
                f.write(f"Average Throughput:  {self.sta1_data['Throughput(Mbps)'].mean():.3f} Mbps\n")
                f.write(f"Average PDR:         {self.sta1_data['PDR(%)'].mean():.2f}%\n")
                f.write(f"Average Loss:        {self.sta1_data['Loss(%)'].mean():.2f}%\n")
                f.write(f"Average Delay:       {self.sta1_data['Delay(ms)'].mean():.3f} ms\n")
                f.write(f"Average RSSI:        {self.sta1_data['RSSI(dBm)'].mean():.2f} dBm\n\n")
            
            # STA2 Summary
            f.write("[STA2 - MOBILE STATION]\n")
            f.write("-"*70 + "\n")
            if len(self.sta2_data) > 0:
                f.write(f"Distance Range:      {self.sta2_data['Distance(m)'].min():.2f} - "
                       f"{self.sta2_data['Distance(m)'].max():.2f} m\n")
                f.write(f"Average Throughput:  {self.sta2_data['Throughput(Mbps)'].mean():.3f} Mbps\n")
                f.write(f"Average PDR:         {self.sta2_data['PDR(%)'].mean():.2f}%\n")
                f.write(f"Average Loss:        {self.sta2_data['Loss(%)'].mean():.2f}%\n")
                f.write(f"Average Delay:       {self.sta2_data['Delay(ms)'].mean():.3f} ms\n")
                f.write(f"Average RSSI:        {self.sta2_data['RSSI(dBm)'].mean():.2f} dBm\n\n")
                
                # AI Decision Summary
                f.write("[AI DECISION SUMMARY]\n")
                f.write("-"*70 + "\n")
                decision_counts = self.sta2_data['AI_Decision'].value_counts()
                for decision, count in decision_counts.items():
                    f.write(f"{decision}: {count} times\n")
        
        print(f"✓ Saved summary report: {report_path}")
    
    def run_complete_analysis(self):
        """Run complete analysis pipeline"""
        print("\n" + "="*70)
        print("STARTING FTM ADAPTIVE WIFI ANALYSIS")
        print("="*70)
        
        self.analyze_performance()
        self.ai_recommendations()
        self.visualize_results()
        self.export_summary_report()
        
        print("\n" + "="*70)
        print("ANALYSIS COMPLETE")
        print("="*70)
        print("\nGenerated files in 'result/' folder:")
        print("  ✓ ftm_analysis.png - Main performance visualization")
        print("  ✓ ai_decision_timeline.png - AI decision timeline")
        print("  ✓ ftm_summary_report.txt - Text summary report")
        print("\n")

def main():
    """Main execution"""
    print("\n" + "="*70)
    print("FTM ADAPTIVE WIFI AI ANALYZER")
    print("="*70)
    print("This tool analyzes NS-3 simulation results and provides")
    print("AI-driven recommendations for WiFi optimization.\n")
    
    analyzer = FTMAnalyzer()
    analyzer.run_complete_analysis()

if __name__ == "__main__":
    main()
