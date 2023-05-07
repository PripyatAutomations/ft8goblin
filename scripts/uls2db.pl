#!/usr/bin/perl
# Why this so slow? :(
# Anyways, here we import already downloaded and unpacked FCC ULS (Universal License System) records for ham and GMRS into SQL.
# XXX: ToDo - Finish the code for confirming the counts after finished

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

# Delete existing database...
unlink($db);

# And open a fresh one
my $dbh = DBI->connect("dbi:SQLite:dbname=$db", "", "");

# Open the SQL statements and read them to memory
open(my $sql_fh, "<", $sqlfile) or die "Cannot open sql $sqlfile: $!";
my $sql_buffer = "";
while (my $line = <$sql_fh>) {
   chomp($line);
   $sql_buffer .= $line . "\n";
}
close($sql_fh);

#print $sql_buffer;

# Execute the SQL statement or die
$dbh->do($sql_buffer) or die "SQL error: " . $DBI::errstr;

####################################
# Import the various records sets: #
####################################
my @datasets = ( "AM", "EN" );
#my @datasets = ( "AM" );
# the data in "CO", "EN", "HD", "HS", "LA", "SC", "SF" aren't really important..
my $dataset_counts = { };
my $dataset_errors = { };
my $dataset_warnings = { };
my $data_dir = "data-sources/fcc-uls/fcc_uls_amateur";

# SQL statements used below, prepared once for efficiency...
my $am_insert_sql = "INSERT INTO fcc_uls (unique_system_identifier, uls_file_num, ebf_number, callsign, operator_class, group_code, ";
   $am_insert_sql .= "region_code, trustee_callsign, trustee_indicator, physician_certification, ve_signature, systematic_callsign_change, ";
   $am_insert_sql .= "vanity_callsign_change, vanity_relationship, previous_callsign, previous_operator_class, trustee_name) ";
   $am_insert_sql .= "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
my $am_insert_stmt = $dbh->prepare($am_insert_sql) or die "Failed preparing AC INSERT statement!";
my $en_update_sql = "UPDATE fcc_uls SET entity_type = ?, licensee_id = ?, entity_name = ?, first_name = ?,";
   $en_update_sql .= " mi = ?, last_name = ?, suffix = ?, phone = ?, fax = ?, email = ?,";
   $en_update_sql .= " street_address = ?, city = ?, state = ?, zip_code = ?, po_box = ?,";
   $en_update_sql .= " attention_line = ? , sgin = ?, frn = ?, applicant_type_code = ?,";
   $en_update_sql .= " applicant_type_other = ?, status_code = ?, lic_category_code = ?,";
   $en_update_sql .= " linked_license_id = ?, linked_callsign = ? WHERE unique_system_identifier = ?;";
my $en_update_stmt = $dbh->prepare($en_update_sql) or die "Failed preparing EN UPDATE statement!\n";

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
        my $unique_system_identifier = $record[1];
        my $uls_file_num = $record[2];
        my $ebf_number = $record[3];
        my $callsign = $record[4];
        my $operator_class = $record[5];
        my $group_code = $record[6];
        my $region_code = $record[7];
        my $trustee_callsign = $record[8];
        my $trustee_indicator = $record[9];
        my $physician_certification = $record[10];
        my $ve_signature = $record[11];
        my $systematic_callsign_change = $record[12];
        my $vanity_callsign_change = $record[13];
        my $vanity_relationship = $record[14];
        my $previous_callsign = $record[15];
        my $previous_operator_class = $record[16];
        my $trustee_name = $record[17];

        # Insert the record into SQL...
        $am_insert_stmt->execute($unique_system_identifier, $uls_file_num, $ebf_number, $callsign, $operator_class,
                                 $group_code, $region_code, $trustee_callsign, $trustee_indicator, $physician_certification,
                                 $ve_signature, $systematic_callsign_change, $vanity_callsign_change, $vanity_relationship,
                                 $previous_callsign, $previous_operator_class, $trustee_name) or die "Failed executing INSERTing record for $uls_file_num from AM\n";
     } elsif ($dataset =~ m/^EN$/) {
        # Here we update that database to add address
        my $unique_system_identifier = $record[1];
        my $uls_file_num = $record[2];
        my $ebf_number = $record[3];
        my $callsign = $record[4];
        my $entity_type = $record[5];
        my $licensee_id = $record[6];
        my $entity_name = $record[7];
        my $first_name = $record[8];
        my $mi = $record[9];
        my $last_name = $record[10];
        my $suffix = $record[11];
        my $phone = $record[12];
        my $fax = $record[13];
        my $email = $record[14];
        my $street_adress = $record[15];
        my $city = $record[16];
        my $state = $record[17];
        my $zip_code = $record[18];
        my $po_box = $record[19];
        my $attention_line = $record[20];
        my $sgin = $record[21];
        my $frn = $record[22];
        my $applicant_type_code = $record[23];
        my $applicant_type_other = $record[24];
        my $status_code = $record[25];
        my $lic_category_code = $record[26];
        my $linked_license_id = $record[27];
        my $linked_callsign = $record[28];
        print "processing EN record for callsign $callsign\n";
        print "et: $entity_type, lid: $licensee_id, en: $entity_name, fn: $first_name,";
        print "mi: $mi, ln: $last_name, su: $suffix, ph: $phone, fa: $fax, em: $email, sa: $street_adress, ci: $city, st: $state,";
        print "zc: $zip_code, pb: $po_box, al: $attention_line, sg: $sgin, fr: $frn, ac: $applicant_type_code,";
        print "ao: $applicant_type_other, sc: $status_code, lc: $lic_category_code, li: $linked_license_id,";
        print "lc: $linked_callsign, us: $unique_system_identifier)\n";

        $en_update_stmt->execute($entity_type, $licensee_id, $entity_name, $first_name, $mi,
           $last_name, $suffix, $phone, $fax, $email, $street_adress, $city, $state,
           $zip_code, $po_box, $attention_line, $sgin, $frn, $applicant_type_code,
           $applicant_type_other, $status_code, $lic_category_code, $linked_license_id,
           $linked_callsign, $unique_system_identifier) or die "Failed UPDATEing record for $frn from EN data\n";
     } else {
       die "Unknown dataset $dataset, bailing"
     }
     $buffer .= $line . "\n";
   }
   printf "COMMITing dataset $dataset\n";
   $dbh->do("COMMIT;");
   close($data_fh);
}

# XXX: We need to load the counts file
#open(my $counts_fh, "<", "$data_dir/counts") or die "Cannot open ULS counts: $file: $!";
#my $lines = 0;
#while (my $line = <$counts_fh>) {
#   $lines++;
#
#   # skip the first line, it's a header...
#   if ($lines == 1) {
#      next;
#   }
#
#   my $exploded = split(" ", $line);
#   my $fn = $exploded[1];
#   my $cnt = $exploded[0];
#   $dataset_counts[$fn] = $cnt;
#   die "fn: $fn has $cnt entries\n";
#}
#close($counts_fh);

# XXX: And make sure we got that many entries for each?

# Disconnect from database
$dbh->disconnect();
