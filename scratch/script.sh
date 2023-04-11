PlotAndSaveP2p()
{
    ./2d_plotter.py s results/p2p_cwnd* $1
    rm results/p2p_cwnd*
}
PlotAndSaveWifi()
{
    ./2d_plotter.py s results/wifi_cwnd* $1
    rm results/wifi_cwnd*
}

cd /home/agrim/repos/ns3/ns-3-dev-git
find results -type f -delete

./ns3 run "scratch/p2p_simul --numnodes=1 --tcpmode=TcpCubic --runtime=50 --pcap=false"
PlotAndSaveP2p cwndP2pCubic1n.jpg
./ns3 run "scratch/p2p_simul --numnodes=4 --tcpmode=TcpCubic --runtime=50 --pcap=false"
PlotAndSaveP2p cwndP2pCubic4n.jpg
./ns3 run "scratch/p2p_simul --numnodes=1 --tcpmode=TcpBbr --runtime=50 --pcap=false"
PlotAndSaveP2p cwndP2pBbr1n.jpg
./ns3 run "scratch/p2p_simul --numnodes=4 --tcpmode=TcpBbr --runtime=50 --pcap=false"
PlotAndSaveP2p cwndP2pBbr4n.jpg

./ns3 run "scratch/wifi_simul --numnodes=1 --tcpmode=TcpCubic --runtime=50 --pcap=false"
PlotAndSaveWifi cwndWifiCubic1n.jpg
./ns3 run "scratch/wifi_simul --numnodes=4 --tcpmode=TcpCubic --runtime=50 --pcap=false"
PlotAndSaveWifi cwndWifiCubic4n.jpg
./ns3 run "scratch/wifi_simul --numnodes=1 --tcpmode=TcpBbr --runtime=50 --pcap=false"
PlotAndSaveWifi cwndWifiBbr1n.jpg
./ns3 run "scratch/wifi_simul --numnodes=4 --tcpmode=TcpBbr --runtime=50 --pcap=false"
PlotAndSaveWifi cwndWifiBbr4n.jpg