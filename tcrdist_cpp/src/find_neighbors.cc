#include "types.hh"
#include "tcrdist.hh"
#include <random>

// a paired tcr with gene-level (actually allele level) resolution
// DistanceTCR_g is defined in tcrdist.hh
typedef std::pair< DistanceTCR_g, DistanceTCR_g > PairedTCR;

// silly helper function
Size
get_tsv_index(
	string const & headerline,
	strings const & fields
)
{
	strings const header(split_to_vector(headerline, "\t"));
	for ( string const & field : fields ) {
		if ( has_element(field, header) ) {
			return vector_index(field, header);
		}
	}
	cerr << "tcrs .tsv file is missing column. Possible fields ";
	for ( string const & field : fields ) cerr << ' '<< field;
	cerr << endl;
	exit(1);
	return 0;
}


//////////////////////////////////// READ THE TCRS
void
read_paired_tcrs_from_tsv_file(
	string const filename,
	TCRdistCalculator const & atcrdist,
	TCRdistCalculator const & btcrdist,
	vector< PairedTCR > & tcrs
)
{
	ifstream data(filename.c_str());
	if ( !data.good() ) {
		cerr<< "unable to open " << filename << endl;
		exit(1);
	}

	string line;
	getline(data, line);
	strings const header(split_to_vector(line, "\t") ); // for debugging only
	Size const va_index(get_tsv_index(line, split_to_vector("va va_gene")));
	Size const vb_index(get_tsv_index(line, split_to_vector("vb vb_gene")));
	Size const cdr3a_index(get_tsv_index(line, split_to_vector("cdr3a")));
	Size const cdr3b_index(get_tsv_index(line, split_to_vector("cdr3b")));

	while ( getline( data, line ) ) {
		strings const l(split_to_vector(line, "\t"));
		if ( l.size() != header.size() ) {
			cerr << "bad line length: " << line << endl;
			exit(1);
		}
		string const va(l[va_index]), vb(l[vb_index]), cdr3a(l[cdr3a_index]),
			cdr3b(l[cdr3b_index]);
		if ( !atcrdist.check_cdr3_ok(cdr3a) ) {
			cerr << "bad cdr3a: " << cdr3a << endl;
			exit(1);
		}
		if ( !btcrdist.check_cdr3_ok(cdr3b) ) {
			cerr << "bad cdr3b: " << cdr3b << endl;
			exit(1);
		}
		if ( !atcrdist.check_v_gene_ok(va)) {
			cerr << "bad va_gene: " << va << endl;
			exit(1);
		}
		if ( !btcrdist.check_v_gene_ok(vb)) {
			cerr << "bad vb_gene: " << vb << endl;
			exit(1);
		}

		tcrs.push_back(make_pair( atcrdist.create_distance_tcr_g(va, cdr3a),
				btcrdist.create_distance_tcr_g(vb, cdr3b)));
	}

	cout << "Read " << tcrs.size() << " paired tcrs from file " << filename << endl;
}


Sizes
read_groups_from_file( string const & filename )
{
	ifstream data(filename.c_str());
	if ( !data.good() ) {
		cerr<< "unable to open " << filename << endl;
		exit(1);
	}
	Sizes groups;
	Size g;
	while ( data.good() ) {
		data >> g;
		if ( !data.fail() ) {
			groups.push_back(g);
		} else {
			break;
		}
	}
	data.close();
	return groups;
}

int main(int argc, char** argv)
{
	try { // to catch tclap exceptions

		TCLAP::CmdLine cmd( "find_neighbors. Use either --num_nbrs or --threshold",' ', "0.1" );

 		TCLAP::ValueArg<Size> num_nbrs_arg("n","num_nbrs",
			"Number of nearest neighbors to find (not including self). Alternative to "
			"using --threshold.", false,
			0, "integer", cmd);

 		TCLAP::ValueArg<int> threshold_arg("t","threshold",
			"TCRdist threshold for neighborness (alternative to using --num_nbrs) -- should be an INTEGER", false,
			-1, "integer", cmd);

		// path to database files
 		TCLAP::ValueArg<std::string> db_filename_arg("d","db_filename",
			"Database file with info for tcrdist calculation", true,
			"", "string",cmd);

 		TCLAP::ValueArg<std::string> outfile_prefix_arg("o","outfile_prefix",
			"Prefix for the knn_indices and knn_distances output files",true,
			"", "string",cmd);


 		TCLAP::ValueArg<string> tcrs_file_arg("f","tcrs_file","TSV (tab separated values) "
			"file containing TCRs for neighbor calculation. Should contain the 4 columns "
			"'va_gene' 'cdr3a' 'vb_gene' 'cdr3b' (or alt fieldnames: 'va' and 'vb')", true,
			"unk", "string", cmd);

 		TCLAP::ValueArg<string> agroups_file_arg("a","agroups_file","np.savetxt output "
			"(ie, one integer per line) ith the agroups information so we can exclude same-group neighbors", false,
			"", "string", cmd);

 		TCLAP::ValueArg<string> bgroups_file_arg("b","bgroups_file","np.savetxt output "
			"(ie, one integer per line) ith the bgroups information so we can exclude same-group neighbors", false,
			"", "string", cmd);

		cmd.parse( argc, argv );

		string const db_filename( db_filename_arg.getValue() );
		Size const num_nbrs( num_nbrs_arg.getValue() );
		int const threshold_int( threshold_arg.getValue() );
		string const tcrs_file( tcrs_file_arg.getValue() );
		string const agroups_file( agroups_file_arg.getValue() );
		string const bgroups_file( bgroups_file_arg.getValue() );
		string const outfile_prefix( outfile_prefix_arg.getValue());

		runtime_assert( ( num_nbrs>0 && threshold_int==-1) || (num_nbrs==0 && threshold_int >=0 ) );

		TCRdistCalculator const atcrdist('A', db_filename), btcrdist('B', db_filename);

		vector< PairedTCR > tcrs;
		read_paired_tcrs_from_tsv_file(tcrs_file, atcrdist, btcrdist, tcrs);

		Size const num_tcrs(tcrs.size());

		Sizes agroups( agroups_file.size() ? read_groups_from_file(agroups_file) : Sizes() );
		Sizes bgroups( bgroups_file.size() ? read_groups_from_file(bgroups_file) : Sizes() );
		if ( agroups.empty() ) {
			for ( Size i=0; i<num_tcrs; ++i ) agroups.push_back(i);
		}
		if ( bgroups.empty() ) {
			for ( Size i=0; i<num_tcrs; ++i ) bgroups.push_back(i);
		}

		runtime_assert( agroups.size() == num_tcrs );
		runtime_assert( bgroups.size() == num_tcrs );
		// two different modes of operations

		Size const BIG_DIST(10000);

		if ( num_nbrs > 0 ) {
			// open the outfiles
			ofstream out_indices(outfile_prefix+"_knn_indices.txt");
			ofstream out_distances(outfile_prefix+"_knn_distances.txt");

			cout << "making " << outfile_prefix+"_knn_indices.txt" << " and " <<
				outfile_prefix+"_knn_distances.txt" << endl;

			Sizes dists(num_tcrs), sortdists(num_tcrs); // must be a better way to do this...
			Sizes knn_indices, knn_distances;
			knn_indices.reserve(num_nbrs);
			knn_distances.reserve(num_nbrs);

			minstd_rand0 rng(1); // seed
			Sizes shuffled_indices;
			for ( Size i=0; i<num_tcrs; ++i ) shuffled_indices.push_back(i);

			for ( Size ii=0; ii< num_tcrs; ++ii ) {
				if ( ii && ii%100==0 ) cerr << '.';
				if ( ii && ii%5000==0 ) cerr << ' ' << ii << endl;

				// for ties, shuffle so we don't get biases based on file order
				shuffle(shuffled_indices.begin(), shuffled_indices.end(), rng);
				DistanceTCR_g const &atcr( tcrs[ii].first ), &btcr( tcrs[ii].second);
				{
					Size i(0);
					for ( PairedTCR const & other_tcr : tcrs ) {
						// NOTE we round down to an integer here!
						dists[i] = Size( 0.5 + atcrdist(atcr, other_tcr.first) + btcrdist(btcr, other_tcr.second) );
						++i;
					}
				}
				Size const a(agroups[ii]), b(bgroups[ii]);
				for ( Size jj=0; jj< num_tcrs; ++jj ) {
					if ( agroups[jj] == a || bgroups[jj] == b ) dists[jj] = BIG_DIST;
				}
				runtime_assert( dists[ii] == BIG_DIST );
				copy(dists.begin(), dists.end(), sortdists.begin());
				nth_element(sortdists.begin(), sortdists.begin()+num_nbrs-1, sortdists.end());
				Size const threshold(sortdists[num_nbrs-1]);
				Size num_at_threshold(0);
				for ( Size i=0; i<num_nbrs; ++i ) {
					// runtime_assert( sortdists[i] <= threshold ); // for debugging
					if ( sortdists[i] == threshold ) ++num_at_threshold;
				}
				// for ( Size i=num_nbrs; i< num_tcrs; ++i ) { // just for debugging
				// 	runtime_assert( sortdists[i] >= threshold );
				// }
				// if ( ii%500==0 ) {
				// 	cout << "threshold: " << threshold << " num_at_threshold: " <<
				// 		num_at_threshold << " ii: " << ii << endl;
				// }
				knn_distances.clear();
				knn_indices.clear();
				for ( Size i : shuffled_indices ) {
					if ( dists[i] < threshold ) {
						knn_indices.push_back(i);
						knn_distances.push_back(dists[i]);
					} else if ( dists[i] == threshold && num_at_threshold>0 ) {
						knn_indices.push_back(i);
						knn_distances.push_back(dists[i]);
						--num_at_threshold;
					}
				}
				runtime_assert(knn_indices.size() == num_nbrs);
				runtime_assert(knn_distances.size() == num_nbrs);
				// save to files:
				for ( Size j=0; j<num_nbrs; ++j ) {
					if (j) {
						out_indices << ' ';
						out_distances << ' ';
					}
					out_indices << knn_indices[j];
					out_distances << knn_distances[j];
				}
				out_indices << '\n';
				out_distances << '\n';
			}
			cerr << endl;
			// close the output files
			out_indices.close();
			out_distances.close();

		} else { // using threshold definition of nbr-ness
			// open the outfiles
			ofstream out_indices(outfile_prefix+"_nbr"+to_string(threshold_int)+"_indices.txt");
			ofstream out_distances(outfile_prefix+"_nbr"+to_string(threshold_int)+"_distances.txt");

			cout << "making " << outfile_prefix+"_nbr"+to_string(threshold_int)+"_indices.txt" << " and " <<
				outfile_prefix+"_nbr"+to_string(threshold_int)+"_distances.txt" << endl;

			runtime_assert( threshold_int >= 0 );
			Size const threshold(threshold_int);

			Sizes knn_indices, knn_distances;
			knn_indices.reserve(num_tcrs);
			knn_distances.reserve(num_tcrs);

			for ( Size ii=0; ii< num_tcrs; ++ii ) {
				if ( ii && ii%100==0 ) cerr << '.';
				if ( ii && ii%5000==0 ) cerr << ' ' << ii << endl;
				knn_indices.clear();
				knn_distances.clear();

				// for ties, shuffle so we don't get biases based on file order
				DistanceTCR_g const &atcr( tcrs[ii].first ), &btcr( tcrs[ii].second);
				Size const a(agroups[ii]), b(bgroups[ii]);
				for ( Size jj=0; jj< num_tcrs; ++jj ) {
					Size const dist( 0.5 + atcrdist(atcr, tcrs[jj].first) + btcrdist(btcr, tcrs[jj].second) );
					if ( dist <= threshold && agroups[jj] != a && bgroups[jj] != b ) {
						knn_indices.push_back(jj);
						knn_distances.push_back(dist);
					}
				}

				// save to file: note that these lines may be empty!!!
				for ( Size j=0; j<knn_indices.size(); ++j ) {
					if (j) {
						out_indices << ' ';
						out_distances << ' ';
					}
					out_indices << knn_indices[j];
					out_distances << knn_distances[j];
				}
				out_indices << '\n';
				out_distances << '\n';
			}
			cerr << endl;
			// close the output files
			out_indices.close();
			out_distances.close();
		}

	} catch (TCLAP::ArgException &e)  // catch any exceptions
		{ std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl; }

}
