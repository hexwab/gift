#!/usr/bin/perl -w
use strict;
use Gtk2;
use Glib;

eval 'use Gnome2';
my $gnome2=!$@;
#print "gnome2\n" if $gnome2;

use giFT::Daemon;
use giFT::Search;
use giFT::TransferHandler;
use giFT::Stats;

my $appname="TregFlip";
my $appver='0.0.1';

Gtk2->init;
Gnome2::Program->init($appname, $appver, undef) if $gnome2;

my $windows=1;
my @realms=('Auto','Text','User','Hash');
my $current_realm='Auto';

my %fields=(
	    name=>{
		name=>'Filename',
		width=>400,
		sort=>sub {$_[1] cmp $_[2]},
		contents=>sub {$_[0]->{file}||$_[0]->{path}},
	    },
	    tname=>{
		name=>'Filename',
		width=>300,
		sort=>sub {s#^(.*)/$# $1# and die for (@_);$_[1] cmp $_[2]},
		contents=>sub {($_[0]->{file}||$_[0]->{path})=~m'/([^/]*)$';$1},
	    },
	    lname=>{
		name=>'Filename',
		width=>350,
		sort=>sub {$_[1] cmp $_[2]},
		contents=>sub {shift->{SOURCE}{url}=~m[://.*?(/.*)];$1},
	    },
	    size=>{
		name=>'Size',
		width=>90,
		sort=>sub {$_[1]<=>$_[2]},
		contents=>sub {shift->{size}},
	    },
	    hash=>{
		name=>'Hash',
		width=>200,
		sort=>sub {$_[1] cmp $_[2]},
		contents=>sub {shift->{hash}},
	    },
	    user=>{
		name=>'User',
		width=>150,
		sort=>sub {$_[1] cmp $_[2]},
		contents=>sub {shift->{user}},
	    },
	    suser=>{
		name=>'User',
		width=>150,
		sort=>sub {$_[1] cmp $_[2]},
		contents=>sub {shift->{SOURCE}{user}},
	    },
	    speed=>{
		name=>'Speed',
		width=>50,
		sort=>sub {$_[1] <=> $_[2]},
		contents=>sub {my $s=shift->{throughput};defined $s or $s=0; format_size($s)},
	    },
	    status=>{
		name=>'Status',
		width=>65,
		sort=>sub {$_[1] cmp $_[2]},
		contents=>sub {shift->{state}},
	    },
	    transfer=>{
		name=>'Transfer',
		width=>150,
		sort=>sub {($_[1].$_[2])=~/\((.*?)%\).*\((.*?)%\)/;$1 <=> $2},
		contents=>sub { my $n=shift->{SOURCE}; return '' if !defined $n;my $t=($n->{total}); my $d=($n->{transmit}); format_size($d).'/'.format_size($t)." (".sprintf("%2.1f%%)",100*($t?$d/$t:1))},
	    },
	    );

my $giftname;
my $giftver;

my $upwidth=0;
my %uploads;

my $users=0;

my $upwin=upload_window();
my $mainwin=main_window();
my $statswin=stats_window();

my $debug=shift;

my $daemon=new giFT::Daemon("localhost",1213,0,$debug);

update_connection();

$daemon->attach($appname,$appver,\&attach_handler);

my $th=new giFT::TransferHandler($daemon,
0,1,
	     \&upload_add,
	     \&upload_change,
	     \&upload_del,
				 );


my $sh=new giFT::Stats($daemon, \&stats_handler, \&update_stats_window);


my $input_id=Glib::IO::add_watch('',$daemon->get_socket->fileno, ['in'], sub{$daemon->poll;1});

Glib::Timeout::add('',3000,sub{$sh->pump;1});
    
Gtk2->main;

# the main window

sub main_window {
    my %w;
    $w{'window'}=my $window=new Gtk2::Window 'toplevel';
    $window->signal_connect("destroy", \&closewin);
    $window->set_title($appname);
    $window->set_resizable(0);


    my ($querybox,$queryentry);
    if ($gnome2) {
	$w{'querybox'}=$querybox=new Gnome2::Entry '';
	$w{'queryentry'}=$queryentry=$querybox->entry;
    } else {
	$w{'querybox'}=$querybox=new Gtk2::Entry;
	$w{'queryentry'}=$queryentry=$querybox;
    };
#    $querybox->set_use_arrows_always(1);
#    $querybox->entry->signal_connect_after('key-press-event',\&do_history);
    $queryentry->signal_connect('activate',\&do_search);
    
    $w{'queryhist'}=[];

    my $menu=new Gtk2::Menu;
    for (0..$#realms) {
	my $item=Gtk2::MenuItem::new_with_label ( '', $realms[$_]);
	my $a=$_;
	$item->signal_connect('activate',sub {$current_realm=$realms[$a]});
	$menu->add($item);
    }
    my $realmmenu=new Gtk2::OptionMenu;
    $realmmenu->set_menu($menu);
    
    my $button = new Gtk2::Button("Search");
    $button->signal_connect("clicked", \&do_search);
    my $hbox=new Gtk2::HBox(0,0);
    $hbox->add($_) for ($realmmenu,$button);
    
    $w{'verlabel'}=my $verlabel=new Gtk2::Label;
    $verlabel->set_padding(4,0);


    my $vbox2=new Gtk2::VBox(0,0);
    my $vbox3=new Gtk2::VBox(0,0);
    my $vbox4=new Gtk2::VBox(0,0);
    my $vbox5=new Gtk2::VBox(0,0);

    my $hbox2=new Gtk2::HBox(0,0);

    my $temp;

    $temp=new Gtk2::Button "Shares";
    $vbox2->add($temp);
    $temp->signal_connect("clicked", sub {start_search('User')});
    $w{shares}=$temp=new Gtk2::Entry;
    $temp->set_editable(0);
    set_width($temp,9);
    $vbox3->add($temp);


    $temp=new Gtk2::Button "Stats";
    $vbox2->add($temp);
    $temp->signal_connect("clicked", sub {open_stats_window()});
    $w{stats}=$temp=new Gtk2::Entry;
    $temp->set_editable(0);
    set_width($temp,9);
    $vbox3->add($temp);

    $temp=new Gtk2::Button "Up";
    $vbox4->add($temp);
#	$temp->set_alignment(1,.5);
    $temp->signal_connect("clicked", \&open_upload_window);

    $temp=new Gtk2::Button "Down";
    $vbox4->add($temp);
#    $temp->signal_connect("clicked", \&open_upload_window);

    $temp=$w{upwidth}=new Gtk2::Entry;
    $temp->set_editable(0);
    set_width($temp,5);
    $vbox5->add($temp);

    $temp=$w{downwidth}=new Gtk2::Entry;
	$temp->set_editable(0);
    set_width($temp,5);
    $vbox5->add($temp);

#    $vbox5->add($upbox);

    $hbox2->add($_) for ($vbox2,$vbox3,$vbox4,$vbox5);

    my $vbox = new Gtk2::VBox(0,0);
    $window->add($vbox);
    $vbox->add($_) for ($querybox,$hbox,$verlabel,$hbox2);

    $queryentry->grab_focus;
    
    $window->show_all;

    \%w;
}

sub do_history {
    # all this is because Gtk2::Combo::set_use_arrows doesn't do what we want :(

    my ($entry,$event)=@_;
    my $combo=$entry->parent;

    use Data::Dumper;


    if ($event->{keyval}==0xFF52) { # GDK_Up
    } elsif ($event->{keyval}==0xFF54) { # GDK_Down
	$combo->set_value_in_list(0,1);
	if ($combo->get_text) {
	}
    }
    print Dumper $combo;
    1;
}

sub attach_handler {
    ($giftver, $giftname)=@_;
    update_connection();
}

sub update_connection {
    $mainwin->{verlabel}->set_text(
				   (defined $giftname)?
				   "$appname $appver => $giftname $giftver":
				   "$appname $appver - not connected"
				   );
    1;
}

# stats...

sub stats_window {
    my %w;

    $w{'window'}=my $window=new Gtk2::Window 'toplevel';
    $w{'open'}=0;
    $window->signal_connect("delete-event", \&fakeclosewin, \%w);

    $window->set_resizable(0);

    \%w;
}

sub update_stats_window {
    # called when a protocol is added or removed

    my @protos=(@_,'');
    
    my @fields=('Proto','Users','Files','Bytes','Avg. files','Avg. shared','Avg. size');
    my $table=new Gtk2::Table(scalar @fields, 1+@protos,0);

    undef $statswin->{proto};

    my $i=0;
    for my $field (@fields) {
	my $temp;
	$temp=new Gtk2::Label $field;
	$temp->set_alignment(.5,.5);
	$table->attach_defaults($temp, $i,$i+1,0,1);
	$i++;
    }

    my $j=1;
    for my $proto (@protos) {
	$i=0;
	my $temp;
	$temp=new Gtk2::Label $proto||'Total';
	$temp->set_alignment(1,.5);
	$temp->set_padding(4,0);
	$table->attach_defaults($temp, $i,$i+1, $j, $j+1);
	$i++;
	for (0..$#fields-1) {
	    my $temp;
	    $statswin->{proto}{$proto}[$_]=$temp=new Gtk2::Entry;
#	    $temp->set_alignment(.5,.5);
	    $temp->set_editable(0);
	    set_width($temp,7);
#	    $temp->set_usize($temp->get_style->font->string_width('1234567'),0);
	    $table->attach_defaults($temp, $i,$i+1,$j,$j+1);
	    $i++;
	}
	$j++;
    }

    $statswin->{table}->destroy if $statswin->{table};

    $statswin->{table}=$table;

    $statswin->{window}->add($table);
    $table->show_all;
}

sub stats_handler {
    my ($files,$size)=$sh->shared_stats;
    for my $proto ($sh->protocols,'') {
	my $entries=$statswin->{proto}{$proto};
	my ($users,$files,$size)=$sh->proto_stats($proto);
	die "Failed to find stats entry for proto '$proto'" if !$entries;
	for (0..5) {
	    $entries->[$_]->set_text(
				     [
				      format_size($users,5,1),
				      format_size($files,5,1),
				      format_size($size,5),
				      $users?format_size($files/$users):'-',
				      $users?format_size($size/$users):'-',
				      $files?format_size($size/$files):'-',
				      ]->[$_]
				     );
	}
    }
    
    my $users=format_size(($sh->proto_stats())[0],3);
    my $protos=$sh->connected_protocols;
    $statswin->{window}->set_title("$users users, $protos protocols - $appname");

    $mainwin->{stats}->set_text("$users users");
    $mainwin->{shares}->set_text(format_size($files,2,1).'/'.format_size($size,3));
}

# search

sub do_search {
    my $query=$mainwin->{queryentry}->get_text;

    my $realm=$current_realm;

#    unshift @{$mainwin->{queryhist}},[$query,$realm];
#    $mainwin->{querybox}->set_popdown_strings(map{$_->[0]} @{$mainwin->{queryhist}});

    if ($realm eq 'Auto') {
	for ($query) {
	    $realm='Hash', last if (/^(\w+:)[0-9a-f]{16,}$/ && /[0-9]/);
	    $realm='User', last if (/([0-9]{1,3}\.){3}/);
	    $realm='Text';
	}
    }

    start_search($realm=>$query);
}

sub start_search {
    my ($realm,$query,$atomic)=@_;
    my $win=new Gtk2::Window 'toplevel';
    $windows++;

    $win->set_title((defined $query?$query:'Local shares')." (in progress) [$realm] - $appname");

    my $tree_view=1;

    my @fields=@{{
	Text=>['name','size','user','hash'],
	Hash=>['name','size','user'],
	User=>['tname','size','hash'],
	}->{$realm}};


    my @widths=map {$fields{$_}{width}} @fields;
    
    $win->set_default_size(
			   do {my $width;$width+=$_ for @widths; $width},
			   300);

    my $store=Gtk2::ListStore->new ('Glib::Scalar',('Glib::String') x @fields);
    my $list=Gtk2::TreeView->new ($store);

    for (0..$#fields) {
	my $col=Gtk2::TreeViewColumn->new_with_attributes(
			       $fields{$fields[$_]}{name},
			       Gtk2::CellRendererText->new,
			       text => $_+1);
	$col->set_resizable(1);
	$col->set_reorderable(1);
	$list->append_column($col);
    
    }
#    $list->set_auto_sort(1);
#    $list->set_selection_mode('extended'); # or 'multiple'

    my $temp=new Gtk2::ScrolledWindow;
    $temp->set_policy('automatic','always');
    $temp->add($list);
    $win->add($temp);
    $win->show_all if !$atomic;

    my $results=0;
    my %results=();
    my %node=();

    my %pathtree=() if $realm eq 'User';

    my $frozen_results=0;
    my $frozen=0;

    my $search;

    my $thawsub=sub {
	return 1 if !$frozen;
#	$list->thaw;
	$frozen_results=0;
	$frozen=0;
	my $results=($realm eq 'Hash')?$search->results:$search->unique_results;
	$win->set_title((defined $query?$query:'Local shares')." ($results so far) [$realm] - $appname");
	1;
    };

  my $thawtimer=Glib::Timeout::add('',1000,$thawsub);
    print STDERR "thawtimer=$thawtimer\n";

#    $list->signal_connect('click-column',sub {
#	my ($list,$col)=@_;
#	if ($list->sort_column!=$col) {
#	    $list->set_sort_type('ascending');
#	} else {
#	    $list->set_sort_type(($list->sort_type eq 'ascending')?'descending':'ascending');
#	}
#
#	$list->set_compare_func($fields{$fields[$col]}{sort});
#	
#	$list->set_sort_column($col);
#	$list->sort;
#	&$thawsub();
#    });

    my $menu_item;

#    my $menu=new Gtk2::Menu;
#    for (
#	 ['Locate file'=>sub { $menu_item && start_search(Hash=>$menu_item->{hash}) }],
#	 ) {
#	my $item=new Gtk2::MenuItem($_->[0]);
#	$menu->append($item);
#	$item->signal_connect('activate',$_->[1]);
#    };
    
#    $menu->show_all;

#    $list->signal_connect("button-press-event", sub {
#		my ($widget,$event)=@_;
#		my $button=$event->{button};
#		return 0 unless $button==3;
#		my ($row,$col)=$list->get_selection_info(@{%$event}{'x','y'});
#		if (defined $row) { 
#		    my $node=$list->node_nth($row);
#		    $menu_item=$list->node_get_row_data($node);
##		    print Dumper ($row,$col,$node,$menu_item);
#		} else {
#		    $menu_item=undef;
#		}
#		$menu->popup(undef,undef,$button,$event->{time},undef);
#		return 1;
#	});

    my $result_handler=sub {
	    my ($search,$m)=@_;
	    my $hash=$m->{hash};
	    my $node;
	    my $parent=undef;

	    if (!$frozen) {
#		$list->freeze;
		$frozen_results=0;
		$frozen=1;
#		$win->set_title("frozen");
	    }
	    

	    for ($realm) {
		last if /Hash/;
		if (/User/ && $tree_view) {
		    my $file=$m->{file} ||$m->{path};
		    my $pos=\%pathtree;
		    $file=~s#(.*)/[^/]*$##;
		    my $path=$1;

		    for (split /\//,$path) {
			my $newpos=$pos->{$_};
			if (!$newpos) {
#			    print "parent=$pos->{''}\n";
			    
			    $newpos=$list->insert($pos->{''},undef,["$_/",'','',''],0,undef,undef,undef,undef,0,1);
			    $pos->{$_}={''=>$newpos};
			    $newpos=$pos->{$_};
			}
			    $pos=$newpos;
		    }

		    $parent=$pos->{''};
		    if (!$parent) {
			my $file=$m->{file} ||$m->{path};
			my $results=$search->results;
			print "file=$file, path=$path, results=$results\n";
#			use Data::Dumper;

#			print Dumper \%pathtree;
		    }
		} else {
		    $parent=$node{$hash}[0];
		}
	    }
		    
#	    unless ($realm eq 'Hash') {
#		    if ((my @tuplets=$search->find_results_by_hash())>1) {
#			for (@tuplets) {
#			    break if $search->data($_);
#			}
#			$parent=$search->data($_);
#		    }
#	    }

	    my $titles=[map {&{$fields{$_}{contents}}($m)} @fields];
	    
#	    $node=$list->insert($parent,undef,$titles,10,undef,undef,undef,undef,0,0);
	    my $row=$store->append;
	    {
		my $i=1;
		$store->set($row,(0,$m,map {$i++,&{$fields{$_}{contents}}($m)} @fields));
	    }
#		$list->collapse($parent) if $parent;
#		$list->collapse($node);
	    push @{$node{$hash}},$node if !$parent;
	    
	    &$thawsub() if ($frozen_results++>50);
	};

    my $finished_handler=sub {
	$list->thaw;
	my $results=($realm eq 'Hash')?$search->results:$search->unique_results;
	$win->set_title((defined $query?$query:'Local shares')." ($results result".(($results==1)?'':'s').") [$realm] - $appname");		
#	Glib::Timeout::remove($thawtimer);
    };

    my $method={
	Text=>'search',
	Hash=>'locate',
	User=>($query?'browse':'shares'),
    }->{$realm};

    $search=new giFT::Search($daemon,$method,$query,$result_handler,$finished_handler,0);

    $win->signal_connect("destroy", sub {
#	$thawtimer->remove unless ($search->finished);

	$search->cancel;

	closewin();
	});

}

# uploads

sub upload_add {
#    my ($m)=values %{(shift)};
    my $m=shift;
    return 1 if $m->{file}=~/nodes\.serve/;
#    use Data::Dumper;
#    print Dumper $m;
    my $id=$m->{''};
    $uploads{$id}=$m;
    my $store=$upwin->{treeview}->get_model;
    my $row=$store->append;
    upload_update($m,$row,$store);
    $m->{_row}=$row;
#    $list->set_row_data($row,\$uploads{$id});

    update_width();
    1;
}

sub upload_change {
#    my ($m)=values %{(shift)};
    my $m=shift;
    return 1 if $m->{file}=~/nodes\.serve/;
    my $id=$m->{''};
    warn "Attempting to change nonexistent upload $id" if (!exists $uploads{$id});
    my @old;
    my $store=$upwin->{treeview}->get_model;
    my $row=$uploads{$id}{_row};
    warn,return if !$row;
    $uploads{$id}=$m;
    upload_update($m,$row,$store);
    update_width();
}

sub upload_update {
    my ($m,$row,$store)=@_;
    my $i=1;
    $store->set($row,(0,$m,map {$i++,&{$fields{$_}{contents}}($m)} @{$upwin->{fields}}));
}

sub upload_del {
    my ($m)=shift;
    return 1 if $m->{file}=~/nodes\.serve/;
    my $id=$m->{''};
    if (exists $uploads{$id}) {
	my $store=$upwin->{treeview}->get_model;
	my $row=$uploads{$id}{_row};
	if (defined $row) {
	    $store->remove($row);
	} else {
	    warn "Unable to locate list entry for upload $id";
	}
	delete $uploads{$id};
    } else {
	warn "Attempting to delete nonexistent upload $id";
    }
    update_width();
}

sub upload_window {
    my %w;
    $w{'window'}=my $win=new Gtk2::Window 'toplevel';

    $w{'open'}=0;

    $win->signal_connect("delete-event", \&fakeclosewin, \%w);

    my @fields=('lname','size','suser','speed','status','transfer','hash');

    $w{'fields'}=\@fields;

    my @widths=map {$fields{$_}{width}} @fields;
    
    $win->set_default_size(
			   do {my $width;$width+=$_ for @widths; $width},
			   300);
#    if (0) {
    $w{'store'}=my $store=Gtk2::ListStore->new ('Glib::Scalar',('Glib::String') x @fields);
    $w{'treeview'}=my $treeview=Gtk2::TreeView->new ($store);

    for (0..$#fields) {
	my $col=Gtk2::TreeViewColumn->new_with_attributes(
			       $fields{$fields[$_]}{name},
			       Gtk2::CellRendererText->new,
			       text => $_+1);
	$col->set_resizable(1);
	$col->set_reorderable(1);
	$treeview->append_column($col);
    
    }
#    {
#	my $i;
#	$list->set_column_width($i++,$_) for (@widths);
#    }
#    $list->set_auto_sort(1);
#    $list->set_selection_mode('extended'); # or 'multiple'

#    $list->signal_connect('click-column',sub {
#	my ($list,$col)=@_;
#	if ($list->sort_column!=$col) {
#	    $list->set_sort_type('ascending');
#	} else {
#	    $list->set_sort_type(($list->sort_type eq 'ascending')?'descending':'ascending');
#	}

#	$list->set_compare_func($fields{$fields[$col]}{sort});
	
#	$list->set_sort_column($col);
#	$list->sort;
#	$list->thaw;# if !$frozen;
#	$list->freeze;# if !$frozen;
#    });

#    my $menu_item;

#    my $menu=new Gtk2::Menu;
#    for (
#	 ['Locate file'=>sub { $menu_item && start_search(Hash=>$menu_item->{hash}) }],
#	 ['Cancel download'=>sub { $menu_item && $menu_item->cancel }],
#	 ) {
#	my $item=Gtk2::MenuItem->new_with_label($_->[0]);
#	$menu->append($item);
#	$item->signal_connect('activate',$_->[1]);
#    };
    
#    $menu->show_all;

#    $list->signal_connect("button-press-event", sub {
#		my ($widget,$event)=@_;
#		my $button=$event->{button};
#		return 0 unless $button==3;
#		my ($row,$col)=$list->get_selection_info(@{$event}{'x','y'});
#		if (defined $row) {
#		    $menu_item=${$list->get_row_data($row)};
##		    print Dumper ($row,$col,$node,$menu_item);
#		} else {
#		    $menu_item=undef;
#		}
#		$menu->popup(undef,undef,$button,$event->{time},undef);
#		return 1;
#	});

    my $temp=new Gtk2::ScrolledWindow;
    $temp->set_policy('automatic','always');
    $temp->add($treeview);
    $temp->show_all;
    $win->add($temp);
#}
    \%w;
}

sub open_upload_window {
    $windows++ if !$upwin->{open};

    $upwin->{window}->show_all;
    $upwin->{open}=1;
}

sub open_stats_window {
    $windows++ if !$statswin->{open};

    $statswin->{window}->show_all;
    $statswin->{open}=1;
}


sub update_width {
    my ($up,$down)=$th->bandwidth;
    my $n=$th->active_uploads;
    my $s=format_size($up);
    $mainwin->{upwidth}->set_text($s);
    my $win=$upwin->{window};
    $win->set_title("Up: $n file".($n==1?'':'s')." @ ${s}B/s - $appname");# if $win;

    $s=format_size($down);
    $mainwin->{downwidth}->set_text($s);
}

# random helper functions

sub format_size {
    my $size=shift;
    my $len=shift||5;
    my $dec=shift;
    my $units=0;
    my $thresh=(10**(($len>3?$len-1:3)));
    my $sf=$len-2;
    while ($size>$thresh) {
	$size/=$dec?1000:1024;
	$units++;
    }
    $units=('','k','M','G','T')[$units];
    my $f=substr(sprintf("%.${sf}f",$size),0,$len);
#    my $f=sprintf("%.${sf}f",$size);
    $f=~s/\.?0*$//;
    $f.$units;
}

sub fakeclosewin {
    shift->hide();
    shift;
    shift->{open}=0;
    closewin();
    1;
}

sub closewin {
    return if --$windows;
    Gtk2->main_quit;
}

sub set_width {
    my ($widget,$len)=@_;
    my $desc=$widget->style->font_desc;

    my $context=$widget->create_pango_layout('123')->get_context;
    my $metrics=$context->get_metrics($desc,$context->get_language);
    my $width=$len*$metrics->get_approximate_digit_width/Gtk2::Pango::scale('');
    my (undef,$height)=$widget->get_size_request;
    $widget->set_size_request($width,$height);
    $widget->unset_flags('can-focus');
}
