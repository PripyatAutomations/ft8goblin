#!/usr/bin/perl
# here we import the various GNIS files and output etc/gnis.db
# XXX: Implement this! This is a copy of uls2db.pl right now
use strict;
use warnings;
use DBI;
use lib '.';
use Text::CSV;
use Data::Dumper;

STDOUT->autoflush(1);

# XXX: Read this from etc/config.json (/etc/ft8goblin too) - cfg:callsign-lookup/fcc-uls-db
my $db = "etc/gnis.db";
my $data_dir = "data-sources/gnis/Text";

# Delete existing database...
print "Removing old database $db\n";
unlink($db);

# And open a fresh one
print "Opening new database $db\n";
my $dbh = DBI->connect("dbi:SQLite:dbname=$db", "", "");

my $create_sql = "
create table gnis (
   place_id INTEGER PRIMARY KEY AUTOINCREMENT,
   gnis_id INT,
   name TEXT,
   class TEXT,
   county TEXT,
   state TEXT,
   country TEXT,
   latitude FLOAT,
   longitude FLOAT,
   src_latitude FLOAT,
   src_longitude FLOAT
);
";

# Execute the SQL statement or die
my $create_stmt = $dbh->prepare($create_sql) or die "create_stmt: prepare faile\n";
$create_stmt->execute() or die "create_stmt: execute\n";

####################################
# Import the various records sets: #
####################################
# SQL statements used below, prepared once for efficiency...
my $insert_sql = "INSERT INTO gnis ( gnis_id, name, class, county, state, " .
                 "country, latitude, longitude, src_latitude, src_longitude ) " .
                 "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
my $insert_stmt = $dbh->prepare($insert_sql) or die "Failed preparing INSERT statement!";

opendir(D, "$data_dir") || die "Can't open directory $data_dir: $!\n";
my @gnis_list_orig = readdir(D);
closedir(D);

# Sort the list
my @gnis_list = sort(@gnis_list_orig);

print "Loading GNIS datasets, this will take quite awhile...\n";

my $total_records = 0;
foreach my $dataset (@gnis_list) {
   my $buffer = "";
   my $sqlbuf = "";
   my $in_lines = 0;

   my $datafile = $data_dir . "/" . $dataset;
   open(my $data_fh, "<", $datafile) or die "Cannot open data file $datafile: $!";

   $dbh->do("BEGIN TRANSACTION");
   
   if ($dataset =~ m/^DomesticNames_([A-Z]{2}).txt$/) {
      my $state = $1;

      print "Processing dataset US/$state\n";

      # Parse domestic dataset
      while (my $line = <$data_fh>) {
         chomp($line);
         $in_lines++;
         my @record = split(/\|/, $line);
         my $f_id = $record[0];
         my $f_name = $record[1];
         my $f_class = $record[2];
         my $f_state = $record[3];
         my $f_country = "USA";
         my $f_county = $record[5];
         my $f_lat = $record[15];
         my $f_lon = $record[16];

         # these seem to be for streams and such
         my $f_src_lat = $record[19];
         my $f_src_lon = $record[20];
#         print "id: $f_id, name: $f_name, class: $f_class, county: $f_county, state: $f_state\n";
#         print "country: $f_country, lat: $f_lat, lon: $f_lon, src_lat: $f_src_lat, src_lon: $f_src_lon\n";

         $insert_stmt->execute($f_id, $f_name, $f_class, $f_county, $f_state, $f_country,
                               $f_lat, $f_lon, $f_src_lat, $f_src_lon)
                                  or die "Failed executing INSERTing record in dataset US/$state \n";
      }

      # decrease by one to account for header
      $in_lines--;
      $total_records += $in_lines;
   } elsif ($dataset =~ m/^Whole_World.txt$/) {
      print "Loading Dataset: World\n";
      # Parse world dataset
      # Parse domestic dataset
      while (my $line = <$data_fh>) {
         chomp($line);
         $in_lines++;
         my @record = split(/\t/, $line);

         if (($in_lines % 10000) == 0) {
            print "Processing record $in_lines in dataset World\n";
         }

         my $f_id = "";
         my $f_name = $record[3];
         my $f_class = "";
         my $f_state = "";
         my $f_country = "";
         my $f_county = "";
         my $f_lat = $record[5];
         my $f_lon = $record[6];

         my $f_src_lat = 0.0;
         my $f_src_lon = 0.0;
#         print "id: $f_id, name: $f_name, class: $f_class, county: $f_county, state: $f_state\n";
#         print "country: $f_country, lat: $f_lat, lon: $f_lon, src_lat: $f_src_lat, src_lon: $f_src_lon\n";

         $insert_stmt->execute($f_id, $f_name, $f_class, $f_county, $f_state, $f_country,
                               $f_lat, $f_lon, $f_src_lat, $f_src_lon)
                                  or die "Failed executing INSERTing record in dataset World \n";
      }

      # decrease by one to account for header
      $in_lines--;
      $total_records += $in_lines;
   }
   $dbh->do("COMMIT;");
   close($data_fh);
}
print "Imported $total_records place names\n";

print "Creating indexes. This will take awhile...\n";

# moved index creaiton down here to save
$dbh->do("BEGIN TRANSACTION");
$dbh->do("CREATE INDEX idx_gnis_place_id ON gnis (place_id);") or die "create idx_gnis_place\n";
$dbh->do("CREATE INDEX idx_gnis_name ON gnis (name);") or die "create idx_gnis_name\n";
$dbh->do("CREATE INDEX idx_gnis_class ON gnis (class);") or die "create idx_gnis_class\n";
$dbh->do("CREATE INDEX idx_gnis_state ON gnis (state);") or die "create idx_gnis_state\n";
$dbh->do("CREATE INDEX idx_gnis_county ON gnis (county);") or die "create idx_gnis_county\n";
$dbh->do("CREATE INDEX idx_gnis_latitude ON gnis (latitude);") or die "create idx_gnis_latitude\n";
$dbh->do("CREATE INDEX idx_gnis_longitude ON gnis (longitude);") or die "create idx_gnis_longitude\n";
$dbh->do("CREATE INDEX idx_gnis_src_latitude ON gnis (src_latitude);") or die "create idx_gnis_src_latitude\n";
$dbh->do("CREATE INDEX idx_gnis_src_longitude ON gnis (src_longitude);") or die "create idx_gnis_src_longitude\n";
$dbh->do("COMMIT");

print "Done!\n";

# Disconnect from database
$dbh->disconnect();
