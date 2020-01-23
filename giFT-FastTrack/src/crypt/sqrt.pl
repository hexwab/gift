for $i (0..255) {
    $j=0;
    $k=0;
    for(;$j<=$i;$j+=2*(++$k)+1) {}
    printf "%x %d\n",$i,$k;
}
