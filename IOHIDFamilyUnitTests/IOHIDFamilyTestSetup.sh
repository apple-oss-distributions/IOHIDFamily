tailspin info | grep "filter descriptor" | cut -d ' ' -f 7 | xargs -I xx  tailspin set ktrace-filter-descriptor xx,S0x0523
tailspin set buffer-size 100
if [ -d "/var/tmp/hidxctest" ]; then
    mv /var/tmp/hidxctest /var/tmp/_hidxctest
fi
exit 0
 
