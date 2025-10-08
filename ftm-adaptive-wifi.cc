/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * FTM-based Adaptive WiFi with AI Integration
 * Compatible with NS-3.33
 * STA1: Static position (5m from AP1)
 * STA2: Dynamic movement (5m -> 20m -> 10m from AP2)
 * created by rahman
 */
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include <iomanip>
#include <map>
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <sys/stat.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FTMAdaptiveWiFi");

// ============== Global Variables ==============
FlowMonitorHelper flowmonHelper;
Ptr<FlowMonitor> monitor;
Ptr<Ipv4FlowClassifier> classifier;
std::ofstream csvOutput;

// Tracking variables
std::map<FlowId, uint64_t> lastRxBytes;
std::map<FlowId, uint64_t> lastTxPackets;
std::map<FlowId, uint64_t> lastRxPackets;
std::map<FlowId, Time> lastDelaySum;

// AP and STA nodes for distance calculation
Ptr<Node> apNode1;
Ptr<Node> apNode2;
Ptr<Node> staNode1;
Ptr<Node> staNode2;

// Current TX power for adaptive control
double currentTxPower1 = 16.0; // dBm
double currentTxPower2 = 16.0; // dBm

// WiFi PHY helpers for dynamic adjustment
YansWifiPhyHelper phy1_global;
YansWifiPhyHelper phy2_global;

// ============== Helper Functions ==============
void CreateResultFolder()
{
    struct stat info;
    if (stat("result", &info) != 0) {
        mkdir("result", 0755);
    }
}

double CalculateDistance(Ptr<Node> node1, Ptr<Node> node2)
{
    Ptr<MobilityModel> mob1 = node1->GetObject<MobilityModel>();
    Ptr<MobilityModel> mob2 = node2->GetObject<MobilityModel>();
    return mob1->GetDistanceFrom(mob2);
}

double CalculateRSSI(double distance, double txPower)
{
    // Friis propagation model at 5GHz
    double frequency = 5.0; // GHz
    double pathLoss = 20 * log10(distance) + 20 * log10(frequency) + 32.44;
    return txPower - pathLoss;
}

std::string ExecuteAIDecision(double distance, double throughput, double rssi)
{
    std::string decision = "maintain";
    double targetThroughput = 4.5; // Target 90% of 5Mbps
    
    // CRITICAL: Check distance and RSSI first (most important)
    if (distance > 15.0 || rssi < -65.0) {
        decision = "increase_power"; // Far distance or weak signal
    } 
    else if (distance > 10.0 || rssi < -60.0) {
        if (throughput < targetThroughput * 0.9) {
            decision = "increase_power"; // Medium distance with degraded throughput
        }
    } 
    else if (distance < 7.0 && rssi > -50.0 && throughput > targetThroughput) {
        decision = "decrease_power"; // Very close with excellent signal - save energy
    }
    
    return decision;
}

void ApplyAIDecision(std::string decision, int apNumber)
{
    if (apNumber == 2) {
        if (decision == "increase_power" && currentTxPower2 < 20.0) {
            currentTxPower2 += 2.0;
            NS_LOG_INFO("AI Decision: Increasing AP2 TX power to " << currentTxPower2 << " dBm");
        } else if (decision == "decrease_power" && currentTxPower2 > 10.0) {
            currentTxPower2 -= 2.0;
            NS_LOG_INFO("AI Decision: Decreasing AP2 TX power to " << currentTxPower2 << " dBm");
        } else if (decision == "increase_power_change_channel") {
            if (currentTxPower2 < 20.0) {
                currentTxPower2 += 3.0;
                NS_LOG_INFO("AI Decision: Aggressive increase AP2 TX power to " << currentTxPower2 << " dBm");
            }
        }
    }
}

void RecordMetrics(double time)
{
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    
    for (FlowMonitor::FlowStatsContainer::const_iterator iter = stats.begin(); 
         iter != stats.end(); ++iter) {
        FlowId fid = iter->first;
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(fid);
        
        if (t.sourceAddress == Ipv4Address("10.1.3.1") || 
            t.sourceAddress == Ipv4Address("10.1.5.1")) {
            
            // Calculate delta metrics
            uint64_t curRxBytes = iter->second.rxBytes;
            uint64_t curTxPackets = iter->second.txPackets;
            uint64_t curRxPackets = iter->second.rxPackets;
            Time curDelaySum = iter->second.delaySum;
            
            uint64_t rxBytesDelta = 0;
            uint64_t txPacketsDelta = 0;
            uint64_t rxPacketsDelta = 0;
            Time delayDelta = Seconds(0);
            
            if (lastRxBytes.find(fid) != lastRxBytes.end()) {
                rxBytesDelta = (curRxBytes >= lastRxBytes[fid]) ? 
                    (curRxBytes - lastRxBytes[fid]) : curRxBytes;
                txPacketsDelta = (curTxPackets >= lastTxPackets[fid]) ? 
                    (curTxPackets - lastTxPackets[fid]) : curTxPackets;
                rxPacketsDelta = (curRxPackets >= lastRxPackets[fid]) ? 
                    (curRxPackets - lastRxPackets[fid]) : curRxPackets;
                delayDelta = (curDelaySum >= lastDelaySum[fid]) ? 
                    (curDelaySum - lastDelaySum[fid]) : curDelaySum;
            } else {
                rxBytesDelta = curRxBytes;
                txPacketsDelta = curTxPackets;
                rxPacketsDelta = curRxPackets;
                delayDelta = curDelaySum;
            }
            
            double throughput = (rxBytesDelta * 8.0 / 1.0) / 1e6; // Mbps
            double pdr = (txPacketsDelta > 0) ? 
                (double)rxPacketsDelta / txPacketsDelta * 100.0 : 0.0;
            double loss = 100.0 - pdr;
            double delay = (rxPacketsDelta > 0) ? 
                (delayDelta.GetSeconds() / rxPacketsDelta) * 1000.0 : 0.0;
            
            // Determine which AP/STA pair
            bool isAP1 = (t.sourceAddress == Ipv4Address("10.1.3.1"));
            Ptr<Node> apNode = isAP1 ? apNode1 : apNode2;
            Ptr<Node> staNode = isAP1 ? staNode1 : staNode2;
            double currentPower = isAP1 ? currentTxPower1 : currentTxPower2;
            
            // Calculate distance and RSSI
            double distance = CalculateDistance(apNode, staNode);
            double rssi = CalculateRSSI(distance, currentPower);
            
            // AI Decision (only for AP2/STA2)
            std::string aiDecision = "maintain";
            if (!isAP1) {
                aiDecision = ExecuteAIDecision(distance, throughput, rssi);
                ApplyAIDecision(aiDecision, 2);
            }
            
            // Write to CSV
            csvOutput << (int)time << ","
                      << (isAP1 ? "AP1-STA1" : "AP2-STA2") << ","
                      << std::fixed << std::setprecision(2) << distance << ","
                      << std::fixed << std::setprecision(3) << throughput << ","
                      << std::fixed << std::setprecision(2) << pdr << ","
                      << std::fixed << std::setprecision(2) << loss << ","
                      << std::fixed << std::setprecision(3) << delay << ","
                      << std::fixed << std::setprecision(2) << rssi << ","
                      << std::fixed << std::setprecision(1) << currentPower << ","
                      << aiDecision << std::endl;
            
            // Update last state
            lastRxBytes[fid] = curRxBytes;
            lastTxPackets[fid] = curTxPackets;
            lastRxPackets[fid] = curRxPackets;
            lastDelaySum[fid] = curDelaySum;
        }
    }
    
    if (time < 20.0) {
        Simulator::Schedule(Seconds(1.0), &RecordMetrics, time + 1.0);
    }
}

// ============== MAIN ==============
int main(int argc, char *argv[])
{
    CreateResultFolder();
    
    CommandLine cmd;
    cmd.Parse(argc, argv);
    
    // Enable logging
    LogComponentEnable("FTMAdaptiveWiFi", LOG_LEVEL_INFO);
    
    NodeContainer allNodes;
    
    // ================= WiFi Group 1 (AP1 + STA1 - Static) =================
    NodeContainer wifiStaNodes1;
    wifiStaNodes1.Create(1);
    staNode1 = wifiStaNodes1.Get(0);
    allNodes.Add(wifiStaNodes1);
    
    NodeContainer wifiApNode1;
    wifiApNode1.Create(1);
    apNode1 = wifiApNode1.Get(0);
    allNodes.Add(wifiApNode1);
    
    YansWifiChannelHelper channel1;
    channel1.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel1.AddPropagationLoss("ns3::LogDistancePropagationLossModel",
                                "Exponent", DoubleValue(3.0),
                                "ReferenceDistance", DoubleValue(1.0));
    
    phy1_global = YansWifiPhyHelper();
    phy1_global.SetChannel(channel1.Create());
    phy1_global.Set("TxPowerStart", DoubleValue(currentTxPower1));
    phy1_global.Set("TxPowerEnd", DoubleValue(currentTxPower1));
    
    WifiHelper wifi1;
    wifi1.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);
    wifi1.SetRemoteStationManager("ns3::MinstrelHtWifiManager");
    
    WifiMacHelper mac1;
    Ssid ssid1 = Ssid("FTM-AP1-5GHz");
    mac1.SetType("ns3::StaWifiMac", 
                 "Ssid", SsidValue(ssid1),
                 "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices1 = wifi1.Install(phy1_global, mac1, wifiStaNodes1);
    
    mac1.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    NetDeviceContainer apDevices1 = wifi1.Install(phy1_global, mac1, wifiApNode1);
    
    // ================= WiFi Group 2 (AP2 + STA2 - Mobile) =================
    NodeContainer wifiStaNodes2;
    wifiStaNodes2.Create(1);
    staNode2 = wifiStaNodes2.Get(0);
    allNodes.Add(wifiStaNodes2);
    
    NodeContainer wifiApNode2;
    wifiApNode2.Create(1);
    apNode2 = wifiApNode2.Get(0);
    allNodes.Add(wifiApNode2);
    
    YansWifiChannelHelper channel2;
    channel2.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel2.AddPropagationLoss("ns3::LogDistancePropagationLossModel",
                                "Exponent", DoubleValue(3.0),
                                "ReferenceDistance", DoubleValue(1.0));
    
    phy2_global = YansWifiPhyHelper();
    phy2_global.SetChannel(channel2.Create());
    phy2_global.Set("TxPowerStart", DoubleValue(currentTxPower2));
    phy2_global.Set("TxPowerEnd", DoubleValue(currentTxPower2));
    
    WifiHelper wifi2;
    wifi2.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);
    wifi2.SetRemoteStationManager("ns3::MinstrelHtWifiManager");
    
    WifiMacHelper mac2;
    Ssid ssid2 = Ssid("FTM-AP2-5GHz");
    mac2.SetType("ns3::StaWifiMac", 
                 "Ssid", SsidValue(ssid2),
                 "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices2 = wifi2.Install(phy2_global, mac2, wifiStaNodes2);
    
    mac2.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    NetDeviceContainer apDevices2 = wifi2.Install(phy2_global, mac2, wifiApNode2);
    
    // ================= P2P and CSMA (Backbone) =================
    NodeContainer routerNode;
    routerNode.Create(1);
    allNodes.Add(routerNode);
    
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
    
    NodeContainer p2pNodes1;
    p2pNodes1.Add(wifiApNode1);
    p2pNodes1.Add(routerNode);
    NetDeviceContainer p2pDevices1 = pointToPoint.Install(p2pNodes1);
    
    NodeContainer p2pNodes2;
    p2pNodes2.Add(wifiApNode2);
    p2pNodes2.Add(routerNode.Get(0));
    NetDeviceContainer p2pDevices2 = pointToPoint.Install(p2pNodes2);
    
    NodeContainer csmaNodes;
    csmaNodes.Add(routerNode.Get(0));
    csmaNodes.Create(1);
    allNodes.Add(csmaNodes.Get(1));
    
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);
    
    // ================= Mobility Models =================
    MobilityHelper mobilitySta1;
    mobilitySta1.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(25.0),
                                      "MinY", DoubleValue(20.0),
                                      "DeltaX", DoubleValue(5.0),
                                      "DeltaY", DoubleValue(5.0),
                                      "GridWidth", UintegerValue(1),
                                      "LayoutType", StringValue("RowFirst"));
    mobilitySta1.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilitySta1.Install(wifiStaNodes1);
    
    // STA2: Waypoint mobility (5m -> 20m -> 10m)
    MobilityHelper mobilitySta2;
    mobilitySta2.SetMobilityModel("ns3::WaypointMobilityModel");
    mobilitySta2.Install(wifiStaNodes2);
    
    Ptr<WaypointMobilityModel> sta2Mobility = 
        wifiStaNodes2.Get(0)->GetObject<WaypointMobilityModel>();
    sta2Mobility->AddWaypoint(Waypoint(Seconds(0), Vector(25.0, 40.0, 0)));
    sta2Mobility->AddWaypoint(Waypoint(Seconds(5), Vector(25.0, 40.0, 0)));
    sta2Mobility->AddWaypoint(Waypoint(Seconds(10), Vector(25.0, 55.0, 0)));
    sta2Mobility->AddWaypoint(Waypoint(Seconds(15), Vector(25.0, 55.0, 0)));
    sta2Mobility->AddWaypoint(Waypoint(Seconds(20), Vector(25.0, 45.0, 0)));
    
    MobilityHelper mobilityFixed;
    mobilityFixed.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityFixed.Install(wifiApNode1);
    mobilityFixed.Install(wifiApNode2);
    mobilityFixed.Install(routerNode);
    mobilityFixed.Install(csmaNodes.Get(1));
    
    // Set positions
    Ptr<ConstantPositionMobilityModel> ap1Mob = 
        wifiApNode1.Get(0)->GetObject<ConstantPositionMobilityModel>();
    ap1Mob->SetPosition(Vector(20.0, 20.0, 0));
    
    Ptr<ConstantPositionMobilityModel> ap2Mob = 
        wifiApNode2.Get(0)->GetObject<ConstantPositionMobilityModel>();
    ap2Mob->SetPosition(Vector(20.0, 40.0, 0));
    
    Ptr<ConstantPositionMobilityModel> routerMob = 
        routerNode.Get(0)->GetObject<ConstantPositionMobilityModel>();
    routerMob->SetPosition(Vector(30.0, 30.0, 0));
    
    Ptr<ConstantPositionMobilityModel> serverMob = 
        csmaNodes.Get(1)->GetObject<ConstantPositionMobilityModel>();
    serverMob->SetPosition(Vector(50.0, 30.0, 0));
    
    // ================= Internet Stack and Addressing =================
    InternetStackHelper stack;
    stack.Install(allNodes);
    
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces1 = address.Assign(p2pDevices1);
    
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);
    
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces1 = address.Assign(staDevices1);
    Ipv4InterfaceContainer apInterface1 = address.Assign(apDevices1);
    
    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces2 = address.Assign(p2pDevices2);
    
    address.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces2 = address.Assign(staDevices2);
    Ipv4InterfaceContainer apInterface2 = address.Assign(apDevices2);
    
    // ================= Applications =================
    uint16_t port = 5000;
    Address serverAddress(InetSocketAddress(csmaInterfaces.GetAddress(1), port));
    
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", serverAddress);
    ApplicationContainer serverApp = sinkHelper.Install(csmaNodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(21.0));
    
    // STA1 -> Server (5Mbps)
    OnOffHelper onoff1("ns3::UdpSocketFactory", serverAddress);
    onoff1.SetAttribute("DataRate", StringValue("5Mbps"));
    onoff1.SetAttribute("PacketSize", UintegerValue(1024));
    onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer client1 = onoff1.Install(wifiStaNodes1.Get(0));
    client1.Start(Seconds(2.0));
    client1.Stop(Seconds(20.0));
    
    // STA2 -> Server (5Mbps)
    OnOffHelper onoff2("ns3::UdpSocketFactory", serverAddress);
    onoff2.SetAttribute("DataRate", StringValue("5Mbps"));
    onoff2.SetAttribute("PacketSize", UintegerValue(1024));
    onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer client2 = onoff2.Install(wifiStaNodes2.Get(0));
    client2.Start(Seconds(2.0));
    client2.Stop(Seconds(20.0));
    
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    
    // ================= PCAP =================
    phy1_global.EnablePcap("result/ftm-ap1", apDevices1.Get(0), true, true);
    phy2_global.EnablePcap("result/ftm-ap2", apDevices2.Get(0), true, true);
    
    // ================= NetAnim =================
    AnimationInterface anim("result/ftm-wireless-animation.xml");
    anim.SetConstantPosition(wifiApNode1.Get(0), 20, 20);
    anim.SetConstantPosition(routerNode.Get(0), 30, 30);
    anim.SetConstantPosition(wifiApNode2.Get(0), 20, 40);
    anim.SetConstantPosition(csmaNodes.Get(1), 50, 30);
    
    anim.UpdateNodeDescription(wifiStaNodes1.Get(0), "STA1-Static");
    anim.UpdateNodeColor(wifiStaNodes1.Get(0), 255, 0, 0);
    anim.UpdateNodeDescription(wifiStaNodes2.Get(0), "STA2-Mobile");
    anim.UpdateNodeColor(wifiStaNodes2.Get(0), 0, 255, 255);
    anim.UpdateNodeDescription(wifiApNode1.Get(0), "AP1");
    anim.UpdateNodeColor(wifiApNode1.Get(0), 0, 0, 255);
    anim.UpdateNodeDescription(wifiApNode2.Get(0), "AP2");
    anim.UpdateNodeColor(wifiApNode2.Get(0), 255, 128, 0);
    anim.UpdateNodeDescription(routerNode.Get(0), "Router");
    anim.UpdateNodeColor(routerNode.Get(0), 0, 255, 0);
    anim.UpdateNodeDescription(csmaNodes.Get(1), "Server");
    anim.UpdateNodeColor(csmaNodes.Get(1), 255, 255, 0);
    
    anim.EnablePacketMetadata(true);
    
    // ================= Flow Monitor =================
    monitor = flowmonHelper.InstallAll();
    classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    
    // Open CSV file
    csvOutput.open("result/ftm_metrics.csv");
    csvOutput << "Time(s),Flow,Distance(m),Throughput(Mbps),PDR(%),Loss(%),"
              << "Delay(ms),RSSI(dBm),TxPower(dBm),AI_Decision" << std::endl;
    
    // Schedule periodic recording
    Simulator::Schedule(Seconds(2.0), &RecordMetrics, 2.0);
    
    Simulator::Stop(Seconds(21.0));
    
    NS_LOG_INFO("Starting simulation...");
    Simulator::Run();
    
    // ================= Final Summary =================
    monitor->SerializeToXmlFile("result/ftm-flowmon-results.xml", true, true);
    csvOutput.close();
    
    std::cout << "\n=== FTM-based Adaptive WiFi Performance Summary ===\n";
    std::cout << "Configuration: 802.11n (5GHz), DataRate: 5Mbps, PacketSize: 1024 bytes\n";
    std::cout << "STA1: Static (5m from AP1) | STA2: Mobile (5m->20m->10m from AP2)\n\n";
    
    std::cout << std::left
              << std::setw(15) << "Flow"
              << std::setw(18) << "Avg Throughput(Mbps)"
              << std::setw(12) << "PDR(%)"
              << std::setw(12) << "Loss(%)"
              << std::setw(15) << "Avg Delay(ms)" << std::endl;
    
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (FlowMonitor::FlowStatsContainer::const_iterator iter = stats.begin(); 
         iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        if (t.sourceAddress == Ipv4Address("10.1.3.1") || 
            t.sourceAddress == Ipv4Address("10.1.5.1")) {
            
            double duration = iter->second.timeLastRxPacket.GetSeconds() - 
                             iter->second.timeFirstTxPacket.GetSeconds();
            double throughput = (duration > 0) ? 
                (iter->second.rxBytes * 8.0 / duration) / 1e6 : 0;
            double pdr = (iter->second.txPackets > 0) ? 
                (double)iter->second.rxPackets / iter->second.txPackets * 100 : 0;
            double loss = 100.0 - pdr;
            double delay = (iter->second.rxPackets > 0) ? 
                (iter->second.delaySum.GetSeconds() / iter->second.rxPackets) * 1000 : 0;
            
            std::string flowName = (t.sourceAddress == Ipv4Address("10.1.3.1")) ? 
                "AP1-STA1" : "AP2-STA2";
            
            std::cout << std::left
                      << std::setw(15) << flowName
                      << std::setw(18) << std::fixed << std::setprecision(3) << throughput
                      << std::setw(12) << std::fixed << std::setprecision(2) << pdr
                      << std::setw(12) << std::fixed << std::setprecision(2) << loss
                      << std::setw(15) << std::fixed << std::setprecision(3) << delay
                      << std::endl;
        }
    }
    
    std::cout << "\nResults saved to 'result/' folder:\n";
    std::cout << "  - ftm_metrics.csv (detailed metrics per second)\n";
    std::cout << "  - ftm-wireless-animation.xml (NetAnim visualization)\n";
    std::cout << "  - ftm-flowmon-results.xml (FlowMonitor statistics)\n";
    std::cout << "  - ftm-ap1-*.pcap and ftm-ap2-*.pcap (packet captures)\n\n";
    
    Simulator::Destroy();
    return 0;
}
