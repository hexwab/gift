#s/\s*ebx = eax;\n\s*ebx = 0 - ebx;\n\s*ebx = ebx \+ (0x.*?);(\n\s*)eax = call \(ebx/$2eax = call \($1 - eax/g

#s/\s*ebx = (0x.*?);\n\s*eax = (0x.*?) \+ ebx;(\n\s*eax = call \()eax/$3.sprintf("0x%x",hex($1)+hex($2))/eg

#s/\s*ecx = (.*);(\n\s*)eax = ROR \(eax, ecx\);/$2eax = ROR (eax, $1);/g
#s/\n\s*u32\s+ecx;//g 
#s/\n\s*eax = (state.*);(\n\s*)eax = (.*)eax(.*);/$2eax = $3($1)$4;/g
#s/\((\d+) == (.*?)\)/($2 == $1)/g
#s/\s*eax = (.*);(\n\s*state\[.*?\] .*?=) eax;(\n\s*eax)/$2 $1;$3/g
#s/\s*eax = (.*);(\n\s*)return eax;/$2return $1;/g
#for $i (1..40) {s/\n\s*eax = (.*);(\n\s*state\[.*?\] .*?= .*?)eax(.*?);/$2($1)$3;/}
s/\((e..)\)/$1/g; # unbracket regs
s/\((l_[\d_]+)\)/$1/g; # unbracket locals
s/(.*?) = \1 (.*?) /$1 $2= /g; # ?=
#for $i (1..20) {s/\n\s*ebx = (.*);(\n\s*state\[.*?\] .*?= .*?)ebx(.*?);/$2($1)$3;/}
for $i (1..20) {s/\n\s*edx = (.*);(\n\s*state\[.*?\] .*?= .*?)edx(.*?);/$2($1)$3;/}
#s/(\s*)(.*?) (\+|-|<<|>>|\*|\^|\/|\&)= (.*?);/$1$2 = $2 $3 ($4);/g # not ?=
s/= \(([^)]*?)\);/= $1;/g; # unbracket exprs
s/\((state\[(\d+)\])\)/$1/g; # unbracket state
#s/\((0x[0-9a-f]+)\)/$1/g; # unbracket consts
s/\(RO(. \([^)]+\))\)/RO$1/g; # unbracket rotates
s/\(RO(. \([^)]+\))\)/RO$1/g; # unbracket rotates
#for $i (1..200) {s/\n\s*local_u32_7 = (.*);(\n\s*state\[.*?\] .*?= .*?)local_u32_7(.*?);/$2($1)$3;/}
#for $i (1..40) {s/\n\s*ecx = (.*);(\n\s*state\[.*?\] .*?= .*?)ecx(.*?);/$2($1)$3;/}
s/\(([^(]*) \+ ([^(]*)\) (\+|-) /$1 + $2 $3 /g; # unbracket adds/subs

s/\(([^(]*) \^ ([^(]*)\) \^ /$1 ^ $2 ^ /g; # unbracket xor
s/\(([^(]*) \| ([^(]*)\) \| /$1 | $2 | /g; # unbracket or
s/\(([^(]*) \& ([^(]*)\) \& /$1 & $2 & /g; # unbracket or
s/\(([^(]*) \* ([^(]*)\) \* /$1 * $2 * /g; # unbracket mul

s/state\[(.*?)\] = (.*) (.) state\[\1\];/state[$1] $3= $2;/g;
s/state\[(.*?)\] = state\[\1\] (.)/state[$1] $2=/g; # state ?=

#s/\s*ecx = (.*);(\n\s*...) = ROR \((.*?), ecx\);/$2 = ROR ($3, $1);/g
#for $i (1..500) {s/\n\s*(...) = (.*);(\n\s*)\1 = (.*)\1(.*);/$3$1 = $4($2)$5;/}
#for $i (1..40) {s/\n\s*local_u32_1 = (.*);(\n\s*state\[.*?\] .*?= .*?)local_u32_1(.*?);/$2($1)$3;/}
#BEGIN{$/='u32 mix_'}$a++;s/local_u32_(.)/'local_u32_'.$a.'_'.$1/eg # uniqify local names
#for $i (8) {
#    for $j (1) {
#	s/local_u32_$i_$j = (.*);\n\s*(state\[.*?\]) = \1;/$2 = $1;/ or die;
#    ($a,$b,$c)=(undef,$1,$2);
#	s/u32\s*local_u32_$i_$j;//g;s/local_u32_$a(?!\d)/$c/g;
#    }}
#s/local_u32_(\d+)_(\d+)/l_$1_$2/g;
#BEGIN{$/='u32 mix_'}next if 1..2;
#s/\n\s*return.*;//g; s/\nu32/\nvoid/g; s/static u32/static void/g;
#s/(state.*?=.*\n)[.\n]+\}/$1}/g or die;
#for $i (0..68) {($m{$i})=/mix_minor$i.*
#s/(.*) = ROL \(\1, /ROLEQ ($1, /g
#s/\s*(...) = (.*);(\n\s*.*? = ROR \(.*?,) \1\);/$3 $2);/g
#s/void mix_minor(.*?) \(u32 \*state\)\n{\n\s*(.*);\n}\n/#define mix_minor$1 $2/sg
#s/(mix_minor.*?) \(state\)/$1/g;
#s/void mix_minor(.*?) .*\n{\n\s*(.*)extra_state(.*);\n}\n/#define mix_minor$1(x) $2x$3/g;
#s/(mix_minor.*?) \(state, (.*?)\)/$1 ($2)/g;
#s/mix_minor17 \((0x.*?)\);/sprintf"mix_minor17 (0x%x);",(hex($1)&31)/eg
#s/mix_minor19 \((.*?)\)/state[4] *= $1/g
