#include "ns3/animation-interface.h"
#include "ns3/applications-module.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/mobility-model.h"
#include "ns3/point-to-point-dumbbell.h"
#include "ns3/string.h"

/*
                   Network Topology
                   
n0 -------------\                   /---------------n2
  (p2p)          r1---------------r2 (p2p)
n1 -------------/   p2p bottleneck  \---------------n3
*/


using namespace ns3;

NS_LOG_COMPONENT_DEFINE("p2p_simul");

int n_nodes = 1;

static void
CwndChange(Ptr<OutputStreamWrapper> file, uint32_t oldval, uint32_t newval)
{
    *file->GetStream() << Simulator::Now().GetSeconds() << "," << newval << '\n';
}

static void
TraceCwnd()
{
    AsciiTraceHelper asciiTraceHelper;
    for (int i = 0; i < n_nodes; i++)
    {
        Ptr<OutputStreamWrapper> file =
            asciiTraceHelper.CreateFileStream("results/p2p_cwnd" + std::to_string(i) + ".csv");
        Config::ConnectWithoutContext(
            "/NodeList/" + std::to_string(i+2) + "/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow",
            MakeBoundCallback(&CwndChange, file));
    }
}

// static void
// GoodputChange(Ptr<OutputStreamWrapper> file, Ptr<PacketSink> sink1, double prevBytesThrough)
// {
//     double recvBytes = sink1->GetTotalRx();
//     double throughput = ((recvBytes - prevBytesThrough) * 8);
//     *file->GetStream() << Simulator::Now().GetSeconds() << "," << throughput << std::endl;
//     Simulator::Schedule(MilliSeconds(1.0), &GoodputChange, file, sink1, recvBytes);
// }

// static void
// TraceGoodput(ApplicationContainer* apps)
// {
//     AsciiTraceHelper asciiTraceHelper;
//     for (int i = 0; i < n_nodes; i++)
//     {
//         Ptr<OutputStreamWrapper> file =
//             asciiTraceHelper.CreateFileStream("results/p2p_goodput" + std::to_string(i) + ".csv");
//         Simulator::Schedule(
//             MilliSeconds(1.0),
//             MakeBoundCallback(&GoodputChange, file, apps->Get(i)->GetObject<PacketSink>(), 0.0));
//     }
// }


// double lastTime = 0;
// unsigned long totalMessagesSent = 0;

// static void
// TraceTx(Ptr<const Packet> packet)
// {
//     lastTime = Simulator::Now().GetSeconds();
//     totalMessagesSent++;
// }

int
main(int argc, char* argv[])
{
    uint32_t payloadSize = 1472;           /* Transport layer payload size in bytes. */
    std::string dataRate = "100Mbps";      /* Application layer datarate. */
    std::string tcp_mode = "TcpCubic";
    int runtime = 50; // Seconds
    bool enable_log = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("dataRate", "Application data rate", dataRate);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("numnodes", "number of senders in simulation", n_nodes);
    cmd.AddValue("logging", "turn on all Application log components", enable_log);
    cmd.AddValue("tcpmode", "specify the type of tcp socket", tcp_mode);
    cmd.AddValue("runtime", "Time (in Seconds) sender applications run", runtime);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::" + tcp_mode));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(payloadSize));

    PointToPointHelper p2phelper;
    p2phelper.SetChannelAttribute("Delay", StringValue("50ns"));
    p2phelper.SetDeviceAttribute("DataRate", StringValue("11Mbps"));

    PointToPointHelper p2pbottleneckhelper;
    p2pbottleneckhelper.SetChannelAttribute("Delay", StringValue("10ms"));
    p2pbottleneckhelper.SetDeviceAttribute("DataRate", StringValue("20Mbps"));

    PointToPointDumbbellHelper dumbbellhelper(n_nodes, p2phelper, n_nodes, p2phelper, p2pbottleneckhelper);
    

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(dumbbellhelper.GetLeft());
    dumbbellhelper.GetLeft()->GetObject<MobilityModel>()->SetPosition(Vector(50, 50, 0));
    
    mobility.SetPositionAllocator("ns3::UniformDiscPositionAllocator",
        "rho", DoubleValue(20),
        "X", DoubleValue(50),
        "Y", DoubleValue(50)
    );
    for (int i = 0; i < n_nodes; i++)
    {
        mobility.Install(dumbbellhelper.GetLeft(i));
    }

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(dumbbellhelper.GetRight());
    dumbbellhelper.GetRight()->GetObject<MobilityModel>()->SetPosition(Vector(150, 150, 0));
    mobility.SetPositionAllocator("ns3::UniformDiscPositionAllocator",
        "rho", DoubleValue(20),
        "X", DoubleValue(150),
        "Y", DoubleValue(150)
    );
    for (int i = 0; i < n_nodes; i++)
    {
        mobility.Install(dumbbellhelper.GetRight(i));
    }
    InternetStackHelper stack;
    dumbbellhelper.InstallStack(stack);

    Ipv4AddressHelper ipv4left, ipv4bottleneck, ipv4right;
    ipv4left.SetBase("10.1.1.0", "255.255.255.0");
    ipv4bottleneck.SetBase("10.2.1.0", "255.255.255.0");
    ipv4right.SetBase("10.3.1.0", "255.255.255.0");
    
    dumbbellhelper.AssignIpv4Addresses(ipv4left, ipv4right, ipv4bottleneck);

    ApplicationContainer senderApps;
    for (int i = 0; i < n_nodes; i++)
    {
        OnOffHelper server("ns3::TcpSocketFactory", InetSocketAddress(dumbbellhelper.GetRightIpv4Address(i), 800));
        server.SetAttribute("PacketSize", UintegerValue(payloadSize));
        server.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        server.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        server.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
        senderApps.Add(server.Install(dumbbellhelper.GetLeft(i)));
 
        // BulkSendHelper blksender("ns3::TcpSocketFactory",
        //                          InetSocketAddress(dumbbellhelper.GetRightIpv4Address(i), 800));
        // senderApps.Add(blksender.Install(dumbbellhelper.GetLeft(i)));
    }

    ApplicationContainer recvApps;
    for (int i = 0; i < n_nodes; i++)
    {
        PacketSinkHelper pktsink("ns3::TcpSocketFactory",
                                 InetSocketAddress(Ipv4Address::GetAny(), 800));
        recvApps.Add(pktsink.Install(dumbbellhelper.GetRight(i)));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    senderApps.Start(Seconds(0.0));
    recvApps.Start(Seconds(0.0));

    senderApps.Stop(Seconds(runtime));
    recvApps.Stop(Seconds(runtime));

    // Tracing
    p2pbottleneckhelper.EnablePcap("results/p2p_simul", dumbbellhelper.GetLeft()->GetDevice(0), false);

    Simulator::Schedule(Seconds(1.001), &TraceCwnd);
    // Simulator::Schedule(Seconds(1.001), MakeBoundCallback(&TraceGoodput, &recvApps));

    if (enable_log)
    {
        LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
        LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    }

    AnimationInterface anim("../animp2p.xml");
    Simulator::Stop(Seconds(runtime + 1));
    Simulator::Run();

    // std::cout << "Sending rate: " << 512*totalMessagesSent*1.0/(1024*1024*lastTime) << "Mbps\n";
    
    for (int i = 0; i < n_nodes; i++)
    {
        std::cout << "Avg. Goodput (Mbps) for flow " + std::to_string(i) + ": "
                  << recvApps.Get(i)->GetObject<PacketSink>()->GetTotalRx() * 8.0 / (runtime * 1024 * 1024) << '\n';
    }

    Simulator::Destroy();

    return 0;
}
