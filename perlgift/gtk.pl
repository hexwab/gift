#!/usr/bin/perl -w
use Gtk '-init';

use giFT::daemon;

my $appname="TregFlip";

my $windows=1;
my @realms=('Text','User','Hash');
my $current_realm='Text';

#my $id = Gtk->idle_add(\&handler, $data);

my $window = new Gtk::Window;
$window->signal_connect("destroy", \&closewin);
$window->set_title($appname);
$window->set_policy(0,0,0);

my $querybox = new Gtk::Entry;
$querybox->signal_connect('activate',\&do_search);

my $menu=new Gtk::Menu;
for (0..$#realms) {
    my $item=new Gtk::MenuItem $realms[$_];
    my $a=$_;
    $item->signal_connect('activate',sub {$current_realm=$realms[$a]});
    $menu->add($item);
}
my $realmmenu=new Gtk::OptionMenu;
$realmmenu->set_menu($menu);

my $button = new Gtk::Button("Search");
$button->signal_connect("clicked", \&do_search);

my $hbox=new Gtk::HBox(0,0);
$hbox->add($_) for ($realmmenu,$button);

my $vbox = new Gtk::VBox(0,0);
$window->add($vbox);
$vbox->add($_) for ($querybox,$hbox);

$querybox->grab_focus;

$window->show_all;
Gtk->main;


sub do_search {
    my $win=new Gtk::Window;
    $win->signal_connect("destroy", \&closewin);
    $windows++;

    my $query=$querybox->get_text;
    my $realm=$current_realm;
    $win->set_title("$query (in progress) [$realm] - $appname");
    $win->set_default_size(600, 300);
    my @fields=('Filename','Size','User');
    my $list=Gtk::CList->new_with_titles(@fields);
    {
	my $i;
	$list->set_column_width($i++,$_) for (350,100,150);
    }
    $list->set_auto_sort(1);
    $list->set_selection_mode('extended'); # or 'multiple'
    $list->signal_connect('click-column',sub {
	my ($list,$col)=@_;
	if ($list->sort_column!=$col) {
	    $list->set_sort_type('ascending');
	} else {
	    $list->set_sort_type(($list->sort_type eq 'ascending')?'descending':'ascending');
	}
	    
	$list->set_sort_column($col);
	$list->sort;
	print "col=$col\n";
    });

    my $temp=new Gtk::ScrolledWindow;
    $temp->set_policy('never','always');
    $temp->add($list);
    $win->add($temp);
    $win->show_all;

    my $d=new giFT::daemon;
    for ($realm) {
	/Text/ and $d->put({search=>{query=>$query}});
	/Hash/ and $d->put({locate=>{query=>$query}});
	/User/ and $d->put({browse=>{query=>$query}});
    }

    my $results=0;
    my %results=();

    my $sock=$d->get_socket;

    my $input;
    $input=Gtk::Gdk->input_add($sock->fileno, ['read'], 
       sub {
#	   print time."\n";
	   my $update=0;
	   while (my $m=$d->get) {
	       my $file=$m->{ITEM}{file};
	       if ($file) {
		   $list->freeze if !$update;
		   $update=1;
		   if (!exists $results{$m->{ITEM}{hash}}) {
		       $list->append($file,$m->{ITEM}{size},$m->{ITEM}{user});
		       $results++;
		   }
		   push @{$results{$m->{ITEM}{hash}}},$m;
	       } else {
		   $win->set_title("$query ($results results) [$realm] - $appname");		
		   Gtk::Gdk->input_remove($input);
		   undef $d;
		   return 0;
	       }
	   }

	   if ($update) {
	       $win->set_title("$query ($results so far) [$realm] - $appname");
	       $list->thaw;
	   }

	   1;
       },
    );


#    $list->append('foo','bar','baz');
    
}

sub closewin {
    return if --$windows;
    Gtk->main_quit;
}
