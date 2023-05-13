#!/usr/bin/perl
# Anyways, here we import already downloaded and unpacked FCC ULS (Universal License System) records for ham and GMRS into SQL.
# XXX: ToDo - Finish the code for confirming the counts after finished
# It looks like we may need to import a lot more than we just AM and EN 
use strict;
use warnings;
use DBI;
use lib '.';
use Text::CSV;
use Data::Dumper;

STDOUT->autoflush(1);

# XXX: Read this from etc/config.json (/etc/ft8goblin too) - cfg:callsign-lookup/fcc-uls-db
my $db = "etc/fcc-uls.db";
my $sqlfile = "sql/fcc-uls.sql";
#
my @datasets = ( "AM", "EN" );			# the data in "CO", "EN", "HD", "HS", "LA", "SC", "SF" aren't really important..
my @dataset_counts = { };
my $dataset_errors = 0;
my $dataset_warnings = 0;
my $data_dir = "data-sources/fcc-uls/fcc_uls_amateur";

print "Parsing upstream counts...\n";
open(my $counts_fh, "<", "$data_dir/counts") or die "Cannot open ULS counts: $data_dir/counts: $!";
my $lines = 0;
while (my $line = <$counts_fh>) {
   $lines++;

   # skip the first line, it's a header...
   if ($lines == 1) {
      next;
   }

   my @exploded = split(" ", $line);
   print "0: " . $exploded[0] . " 1: " . $exploded[1] . "\n";
   my $cnt = $exploded[0];
   my @f = split("/", $exploded[1]);
   my $fn = $f[1];
#   $dataset_counts[$fn] = $cnt;
#   print "fn: $fn has $cnt entries\n";
}
close($counts_fh);

#die "temp stop\n";

# Delete existing database...
print "Removing old database $db\n";
unlink($db);

# And open a fresh one
print "Opening new database $db\n";
my $dbh = DBI->connect("dbi:SQLite:dbname=$db", "", "");
my $sql_buffer = "";

# Open the SQL statements and read them to memory (use this if using other than sqlite)
#open(my $sql_fh, "<", $sqlfile) or die "Cannot open sql $sqlfile: $!";
#while (my $line = <$sql_fh>) {
#   chomp($line);
#   $sql_buffer .= $line . "\n";
#}
#close($sql_fh);

#print $sql_buffer;

my $sql_create_uls_ham = "
create table uls_ham (
      unique_id  numeric(9,0)         not null,
      uls_file_number              char(14)             null,
      ebf_number                varchar(30)          null,
      callsign                  char(10)             null,
      operator_class            char(1)              null,
      group_code                char(1)              null,
      region_code               tinyint              null,
      trustee_callsign          char(10)             null,
      trustee_indicator         char(1)              null,
      physician_certification   char(1)              null,
      ve_signature              char(1)              null,
      systematic_callsign_change char(1)             null,
      vanity_callsign_change    char(1)              null,
      vanity_relationship       char(12)             null,
      previous_callsign         char(10)             null,
      previous_operator_class   char(1)              null,
      trustee_name              varchar(50)          null
);
";

my $sql_create_uls_frn = "
create table uls_frn (
      unique_id  numeric(9,0)         not null,
      uls_file_number           char(14)             null,
      ebf_number                varchar(30)          null,
      callsign                 char(10)             null,
      entity_type               char(2)              null,
      licensee_id               char(9)              null,
      entity_name               varchar(200)         null,
      first_name                varchar(20)          null,
      mi                        char(1)              null,
      last_name                 varchar(20)          null,
      suffix                    char(3)              null,
      phone                     char(10)             null,
      fax                       char(10)             null,
      email                     varchar(50)          null,
      street_address            varchar(60)          null,
      city                      varchar(20)          null,
      state                     char(2)              null,
      zip_code                  char(9)              null,
      po_box                    varchar(20)          null,
      attention_line            varchar(35)          null,
      sgin                      char(3)              null,
      frn                       char(10)             null,
      applicant_type_code       char(1)              null,
      applicant_type_other      char(40)             null,
      status_code               char(1)		     null,
      status_date		datetime	     null,
      lic_category_code		char(1)		     null,
      linked_license_id		numeric(9,0)	     null,
      linked_callsign		char(10)	     null
);
";

# Execute the SQL statement or die
my $create_uls_ham_stmt = $dbh->prepare($sql_create_uls_ham) or die "create_uls_ham_stmt: prepare faile\n";
$create_uls_ham_stmt->execute() or die "create_uls_ham_stmt: execute\n";
my $create_uls_frn_stmt = $dbh->prepare($sql_create_uls_frn) or die "create_uls_frn_stmt: prepare faile\n";
$create_uls_frn_stmt->execute() or die "create_uls_frn_stmt: execute\n";

####################################
# Import the various records sets: #
####################################

# SQL statements used below, prepared once for efficiency...
my $am_insert_sql = "INSERT INTO uls_ham (unique_id, uls_file_number, ebf_number, callsign, operator_class, group_code, ";
   $am_insert_sql .= "region_code, trustee_callsign, trustee_indicator, physician_certification, ve_signature, systematic_callsign_change, ";
   $am_insert_sql .= "vanity_callsign_change, vanity_relationship, previous_callsign, previous_operator_class, trustee_name) ";
   $am_insert_sql .= "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
my $am_insert_stmt = $dbh->prepare($am_insert_sql) or die "Failed preparing AC INSERT statement!";
my $en_insert_sql = "INSERT INTO uls_frn (unique_id, callsign, entity_type, licensee_id, entity_name,";
   $en_insert_sql .= " first_name, mi, last_name, suffix, phone, fax, email,";
   $en_insert_sql .= " street_address, city, state, zip_code, po_box,";
   $en_insert_sql .= " attention_line , sgin, frn, applicant_type_code,";
   $en_insert_sql .= " applicant_type_other, status_code, lic_category_code,";
   $en_insert_sql .= " linked_license_id, linked_callsign ) VALUES ( ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? );";
my $en_insert_stmt = $dbh->prepare($en_insert_sql) or die "Failed preparing EN INSERT statement!\n";

print "Loading FCC ULS datasets\n";
for my $dataset (@datasets) {
   my $buffer = "";
   my $sqlbuf = "";
   my $in_lines = 0;
   
   my $datafile = $data_dir . "/" . $dataset . ".dat";
   print "datafile: $datafile\n";
   open(my $data_fh, "<", $datafile) or die "Cannot open data file $datafile: $!";

   print "BEGINning transaction on dataset $dataset\n";
   $dbh->do("BEGIN TRANSACTION;");

   while (my $line = <$data_fh>) {
     chomp($line);
     $in_lines++;
     my @record = split(/\|/, $line);

     if (($in_lines % 10000) == 0) {
        print "Processing record $in_lines in dataset $dataset\n";
     }

     if ($dataset =~ m/^AM$/) {
        my $unique_id = $record[1];
        my $uls_file_number = $record[2];
        my $ebf_number = $record[3];
        my $callsign = uc $record[4];
        my $operator_class = uc $record[5];
        my $group_code = uc $record[6];
        my $region_code = uc $record[7];
        my $trustee_callsign = uc $record[8];
        my $trustee_indicator = uc $record[9];
        my $physician_certification = uc $record[10];
        my $ve_signature = uc $record[11];
        my $systematic_callsign_change = uc $record[12];
        my $vanity_callsign_change = uc $record[13];
        my $vanity_relationship = uc $record[14];
        my $previous_callsign = uc $record[15];
        my $previous_operator_class = uc $record[16];
        my $trustee_name = uc $record[17];

        # Insert the record into SQL...
        $am_insert_stmt->execute($unique_id, $uls_file_number, $ebf_number, $callsign, $operator_class,
                                 $group_code, $region_code, $trustee_callsign, $trustee_indicator, $physician_certification,
                                 $ve_signature, $systematic_callsign_change, $vanity_callsign_change, $vanity_relationship,
                                 $previous_callsign, $previous_operator_class, $trustee_name) or die "Failed executing INSERTing record for $uls_file_number from AM\n";
     } elsif ($dataset =~ m/^EN$/) {
        # Here we update that database to add address
        my $unique_id = $record[1];
        my $uls_file_number = $record[2];
        my $ebf_number = $record[3];
        my $callsign = uc $record[4];
        my $entity_type = uc $record[5];
        my $licensee_id = uc $record[6];
        my $entity_name = uc $record[7];
        my $first_name = uc $record[8];
        my $mi = uc $record[9];
        my $last_name = uc $record[10];
        my $suffix = uc $record[11];
        my $phone = uc $record[12];
        my $fax = $record[13];
        my $email = lc $record[14];
        my $street_adress = uc $record[15];
        #
        my $city = uc $record[16];
        my $state = uc $record[17];
        my $zip_code = $record[18];
        my $po_box = uc $record[19];
        my $attention_line = uc $record[20];
        my $sgin = uc $record[21];
        my $frn = $record[22];
        my $applicant_type_code = uc $record[23];
        my $applicant_type_other = uc $record[24];
        my $status_code = uc $record[25];
        my $status_rate = uc $record[26];
        my $lic_category_code = uc $record[27];
        my $linked_license_id = $record[28];
        my $linked_callsign = uc $record[29];

#        print "unique_id: $unique_id\n";
#        print "uls_file_number: $uls_file_number\n";
#        print "ebf_number: $ebf_number\n";
#        print "callsign: $callsign\n";
#        print "licensee_id: $licensee_id\n";
#        print "entity_name: $entity_name\n";
#        print "first_name: $first_name\n";
#        print "mi: $mi\n";
#        print "last_name: $last_name\n";
#        print "suffix: $suffix\n";

        $en_insert_stmt->execute($unique_id, $callsign, $entity_type, $licensee_id, $entity_name, $first_name, $mi,
           $last_name, $suffix, $phone, $fax, $email, $street_adress, $city, $state,
           $zip_code, $po_box, $attention_line, $sgin, $frn, $applicant_type_code,
           $applicant_type_other, $status_code, $lic_category_code, $linked_license_id,
           $linked_callsign) or die "Failed UPDATEing record for $frn from EN data\n";
     } else {
        die "Unknown dataset $dataset, bailing"
     }
     $buffer .= $line . "\n";
   }
   printf "COMMITing dataset $dataset\n";
   $dbh->do("COMMIT;");
   close($data_fh);
}

print "Creating indexes. This will take awhile...\n";

# moved index creaiton down here to save
$dbh->do("BEGIN TRANSACTION");
$dbh->do("CREATE INDEX idx_ham_callsign ON uls_ham (callsign);") or die "create idx_ham_callsign\n";
$dbh->do("CREATE INDEX idx_ham_unique_sys_id ON uls_ham (unique_id);") or die "create idx_ham_unique_sys_id\n";
$dbh->do("CREATE INDEX idx_ham_uls_file_number ON uls_ham (uls_file_number);") or die "create idx_ham_uls_file_number\n";
$dbh->do("CREATE INDEX idx_frn_unique_sys_id ON uls_frn (unique_id);") or die "create idx_frn_unique_sys_id\n";
$dbh->do("CREATE INDEX idx_frn_file_number ON uls_frn (uls_file_number);") or die "create idx_frn_file_number\n";
$dbh->do("CREATE INDEX idx_frn_fname ON uls_frn (first_name);") or die "create idx_frn_fname\n";
$dbh->do("CREATE INDEX idx_frn_lname ON uls_frn (last_name);") or die "create idx_frn_lname\n";
$dbh->do("CREATE INDEX idx_frn_entity_name ON uls_frn (entity_name);") or die "create idx_frn_entity_name\n";
$dbh->do("CREATE INDEX idx_frn_street_addr ON uls_frn (street_address);") or die "create idx_frn_street_addr\n";
$dbh->do("CREATE INDEX idx_frn_city ON uls_frn (city);") or die "create idx_frn_city\n";
$dbh->do("CREATE INDEX idx_frn_state ON uls_frn (state);") or die "create idx_frn_state\n";
$dbh->do("CREATE INDEX idx_frn_zip ON uls_frn (zip_code);") or die "create idx_frn_zip\n";
$dbh->do("CREATE INDEX idx_frn_frn ON uls_frn (frn);") or die "create idx_frn_frn\n";
$dbh->do("COMMIT");

# XXX: Compare counts and make sure we succesfully imported everything
print "Done!\n";

# Disconnect from database
$dbh->disconnect();
