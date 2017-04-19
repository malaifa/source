#!usr/bin/env python

from __future__ import print_function

################################################################################
# A GENERAL EXPLANATION

"""
ligand_interface.py

This script performs interface structure prediction (high-resolution docking)
on a ligand-protein complex. This sample script is very similar to docking.py
without interface location prediction (the low-resolution (centroid) docking
stages). The high-resolution (fullatom) docking stage consists of small
rigid-body perturbations, sidechain packing, and minimization. Without
interface location prediction, this sample script cannot perform full ligand-
protein docking. The "ligand" scoring function is optimized for
ligand-protein docking.

The "Alternate scenarios" section below provides guidelines for using additional
ligand compounds in PyRosetta. A second method,
generate_nonstandard_residue_set, is supplied here to allow this script to run
in older and newer versions of PyRosetta.

Instructions:

1) ensure that your PDB file is in the current directory
2) obtain ATP.params file from /demos/data
3) ***uncomment lines 321-322***
4) run the script:
    from commandline                        >python D120_Ligand_interface.py

    from within python/ipython              [1]: run D120_Ligand_interface.py

Author: Evan H. Baugh
    based on an original script by Sid Chaudhury
    revised and motivated by Robert Schleif

Updated by Boon Uranukul, 6/9/12
Simplified special constant seed initialization ~ Labonte

References:
    J. J. Gray, "High-resolution protein-protein docking," Curr. Opinions in
        Struct. Bio. 16 (2) 183-193 (2006).

"""

################################################################################
# THE BASIC PROTOCOL, sample_ligand_interface

"""
This sample script is setup for usage with
    commandline arguments,
    default running within a python interpreter,
    or for import within a python interpreter,
        (exposing the sample_ligand_interface method)

The method sample_ligand_interface:
1.  creates a pose from the desired PDB file, if ligand_params are specified,
        use these when loading the pose_from_file
2.  sets up the pose FoldTree for docking
3.  creates a copy of the pose to be modified
4.  creates a ScoreFunctions for scoring ligand-protein complexes
5.  sets up the DockMCMProtocol object for fullatom docking
6.  creates a (Py)JobDistributor for managing multiple trajectories
7.  create a PyMOL_Observer for viewing intermediate output
8.  exports the original structure to PyMOL
9.  perform protein-protein docking:
        a. set necessary variables for the new trajectory
            -reset the pose structure to the input conformation
            -change the pose's PDBInfo.name, for exporting to PyMOL
        b. perform Rosetta high-resolution docking
        c. output the decoy structure
            -to PyMOL using the PyMOL_Observer.pymol.apply
            -to a PDB file using the PyJobDistributor.output_decoy

"""

import optparse    # for sorting options

from rosetta import *
from pyrosetta import *

init(extra_options = "-constant_seed")
# normally, init() works fine
# for this sample script, we want to ease comparison by making sure all random
#    variables generated by Rosetta in this instance of PyRosetta start from a
#    constant seed
# here we provide the additional argument "-constant_seed" which sets all the
#    random variables generated by Rosetta from a constant seed (google random
#    seed for more information)
# some options can be set after initialization, please see PyRosetta.org FAQs
#    for more information
# WARNING: option '-constant_seed' is for testing only! MAKE SURE TO REMOVE IT IN PRODUCTION RUNS!!!!!

import os; os.chdir('.test.output')


#########
# Methods

def sample_ligand_interface(pdb_filename, partners,
        ligand_params = [''], jobs = 1, job_output = 'ligand_output' ):
    """
    Performs ligand-protein docking using Rosetta fullatom docking
        (DockingHighRes) on the ligand-protein complex in  <pdb_filename>
        using the relative chain  <partners>  .
        If the ligand parameters (a .params file, see below) are not defaultly
        loaded into PyRosetta,  <ligand_params>  must supply the list of files
        including the ligand parameters.
        <jobs>  trajectories are performed with output structures named
        <job_output>_(job#).pdb.
    """
    # 1. creates a pose from the desired PDB file
    pose = Pose()
    if ligand_params[0]:    # the params list has contents
        ligand_params = Vector1(ligand_params)
        new_res_set = generate_nonstandard_residue_set(ligand_params)
        pose_from_file(pose, new_res_set, pdb_filename)
    else:
        pose_from_file(pose, pdb_filename)

    # 2. setup the docking FoldTree
    # using this method, the jump number 1 is automatically set to be the
    #    inter-body jump
    dock_jump = 1
    # the exposed method setup_foldtree takes an input pose and sets its
    #    FoldTree to have jump 1 represent the relation between the two docking
    #    partners, the jump points are the residues closest to the centers of
    #    geometry for each partner with a cutpoint at the end of the chain,
    # the second argument is a string specifying the relative chain orientation
    #    such as "A_B" of "LH_A", ONLY TWO BODY DOCKING is supported and the
    #    partners MUST have different chain IDs and be in the same pose (the
    #    same PDB), additional chains can be grouped with one of the partners,
    #    the "_" character specifies which bodies are separated
    # the third argument...is currently unsupported but must be set (it is
    #    supposed to specify which jumps are movable, to support multibody
    #    docking...but Rosetta doesn't currently)
    # the FoldTrees setup by this method are for TWO BODY docking ONLY!
    protocols.docking.setup_foldtree(pose, partners, Vector1([dock_jump]))

    # 3. create a copy of the pose for testing
    test_pose = Pose()
    test_pose.assign(pose)

    # 4. create ScoreFunctions for centroid and fullatom docking
    scorefxn = create_score_function('ligand')

    #### global docking, a problem solved by the Rosetta DockingProtocol,
    ####    requires interface detection and refinement
    #### as with other protocols, these tasks are split into centroid (interface
    ####    detection) and high-resolution (interface refinement) methods
    #### without a centroid representation, low-resolution ligand-protein
    ####    prediction is not possible and as such, only the high-resolution
    ####    ligand-protein interface refinement is available
    #### WARNING: if you add a perturbation or randomization step, the
    ####    high-resolution stages may fail (see Changing Ligand Docking
    ####    Sampling below)
    #### a perturbation step CAN make this a global docking algorithm however
    ####    the rigid-body sampling preceding refinement requires extensive
    ####    sampling to produce accurate results and this algorithm spends most
    ####    of its effort in refinement (which may be useless for the predicted
    ####    interface)

    # 5. setup the high resolution (fullatom) docking protocol (DockMCMProtocol)
    # ...as should be obvious by now, Rosetta applications have no central
    #    standardization, the DockingProtocol object can be created and
    #    applied to perform Rosetta docking, many of its options and settings
    #    can be set using the DockingProtocol setter methods
    # there is currently no centroid representation of an arbitrary ligand in
    #    the chemical database, although you can check to see if it is already
    #    present or make your own (see "Obtaining Params Files" below), and
    #    without a centroid representation, the low-resolution docking stages
    #    are not useful for ligand docking
    docking = protocols.docking.DockMCMProtocol()
    docking.set_scorefxn(scorefxn)

    # 6. setup the PyJobDistributor
    jd = PyJobDistributor(job_output, jobs, scorefxn)

    # 7. setup a PyMOL_Observer (optional)
    # the PyMOL_Observer object owns a PyMolMover and monitors pose objects for
    #    structural changes, when changes are detected the new structure is
    #    sent to PyMOL
    # fortunately, this allows investigation of full protocols since
    #    intermediate changes are displayed, it also eliminates the need to
    #    manually apply the PyMolMover during a custom protocol
    # unfortunately, this can make the output difficult to interpret (since you
    #    aren't explicitly telling it when to export) and can significantly slow
    #    down protocols since many structures are output (PyMOL can also slow
    #    down if too many structures are provided and a fast machine may
    #    generate structures too quickly for PyMOL to read, the
    #    "Buffer clean up" message
    # uncomment the line below to use PyMOL_Observer
##    AddPyMolObserver(test_pose, True)

    # 8. perform protein-protein docking
    counter = 0    # for pretty output to PyMOL
    while not jd.job_complete:
        # a. set necessary variables for this trajectory
        # -reset the test pose to original (centroid) structure
        test_pose.assign(pose)
        # -change the pose name, for pretty output to PyMOL
        counter += 1
        test_pose.pdb_info().name(job_output + '_' + str(counter))

        # b. perform docking
        docking.apply(test_pose)

        # c. output the decoy structure:
        # to PyMOL
        test_pose.pdb_info().name(job_output + '_' + str(counter) + '_fa')
        # to a PDB file
        jd.output_decoy(test_pose)

# a method for producing non-standard ResidueTypeSets
#    there is a custom (PyRosetta only) method in PyRosetta v2.0beta named
#    generate_nonstandard_residue_set which will work there, but not for newer
#    versions, this method supports  both older and newer versions of PyRosetta
def generate_nonstandard_residue_set(params_list):
    """
    Returns a "custom" ResidueTypeSet with the normal ResidueTypes and any
        new ones added as a Vector1 of .params filenames,
        the input  <params_list>

    example(s):
        res_set = generate_nonstandard_residue_set( Vector1( ['ATP.params'] ) )
    See Also:
        Pose
        Residue
        ResidueType
        ResidueTypeSet

    """
    res_set = ChemicalManager.get_instance().nonconst_residue_type_set(
        'fa_standard')
    atoms = ChemicalManager.get_instance().atom_type_set('fa_standard')
    mm_atoms = ChemicalManager.get_instance().mm_atom_type_set('fa_standard')
    orbitals = ChemicalManager.get_instance().orbital_type_set('fa_standard')

    # element_sets were added to the chemical database and changed the syntax
    #    of read_files
    elements = ChemicalManager.get_instance().element_set('default')
    res_set.read_files(params_list, atoms, elements, mm_atoms, orbitals)

    return res_set

################################################################################
# INTERPRETING RESULTS

"""
The (Py)JobDistributor will output the lowest scoring pose for each trajectory
(as a PDB file), recording the score in <job_output>.fasc. Generally,
the decoy generated with the lowest score contains the best prediction
for the protein conformation. PDB files produced from docking will contain
both docking partners in their predicted conformation. When inspecting these
PDB files (or the PyMOL_Observer output) be aware that PyMOL can introduce or
predict bonds that do not exist, particularly for close atoms. This rarely
occurs when using the PyMolMover.keep_history feature (since PyRosetta will
sample some conformation space that has clashes).

The PyMOL_Observer will output a series of structures directly produced by the
DockingProtocol. Unfortunately, this may include intermediate structures that
do not yield any insight into the protocol performance. A LARGE number of
structures are output to PyMOL and your machine may have difficulty
loading all of these structures. If this occurs, try changing the
PyMOL_Observer keep_history to False or running the protocol without the
PyMOL_Observer.
Interface structure prediction is useful for considering what physical
properties are important in the binding event and what conformational changes
occur. Once experienced using PyRosetta, you can easily write scripts to
investigate the Rosetta score terms and structural characteristics. There is no
general interpretation of ligand-binding results. Although Rosetta score does
not translate directly to physical meaning (it is not physical energy),
splitting the docked partners and comparing the scores (after packing or
refinement) can indicate the strength of the bonding interaction
(unfortunately, the pose manipulation tools in PyRosetta are currently
undergoing repairs though manually splitting a PDB is very easy).

"""

################################################################################
# COMMANDLINE COMPATIBILITY

# everything below is added to provide commandline usage,
#   the available options are specified below
# this method:
#    1. defines the available options
#    2. loads in the commandline or default values
#    3. calls dna_sample_ligand_interface with these values

# parser object for managing input options
# all defaults are for the example using "test_lig.pdb" with reduced
#    cycles/jobs to provide results quickly
parser = optparse.OptionParser()
parser.add_option('--pdb_filename', dest = 'pdb_filename',
    default = '../demos/data/test_lig.pdb',    # default example PDB
    help = 'the PDB file containing the ligand and protein to dock')
# for more information on "partners", see sample_docking step 2.
parser.add_option('--partners', dest = 'partners',
    default = 'E_X',    # default for the example test_lig.pdb
    help = 'the relative chain partners for docking')
# ligand options
parser.add_option('--ligand_params', dest = 'ligand_params' ,
    default = 'ATP.params' ,    # default for the example test_lig.pdb
    help = 'the ligand residue parameter file')
# PyJobDistributor options
parser.add_option('--jobs', dest='jobs',
    default = '1',    # default to single trajectory for speed
    help = 'the number of jobs (trajectories) to perform')
parser.add_option('--job_output', dest = 'job_output',
    default = 'ligand_output',    # if a specific output name is desired
    help = 'the name preceding all output, output PDB files and .fasc')
(options,args) = parser.parse_args()

# PDB file option
pdb_filename = options.pdb_filename
partners = options.partners
# ligand options
ligand_params = options.ligand_params.split(',')
# JobDistributor options
jobs = int(options.jobs)
job_output = options.job_output

# uncomment the command line below to run this demo. Make sure you have already
#      placed the ATP.params file into (in PyRosetta main directory)
#/rosetta_database/chemical/residue_type_sets/fa_standard/residue_types

'''sample_ligand_interface(pdb_filename, partners, ligand_params,
    jobs, job_output)
'''

################################################################################
# ALTERNATE SCENARIOS

#######################################
# Obtaining and Preparing Ligand PDB files
"""
PDB files are the keys to structural Bioinformatics and structure prediction.
PDB files are most easily obtained from the RCSB website but may contain
variability which makes them incompatible with PyRosetta. Ligands are tricky
since PyRosetta must know what ResidueType the ligand is. There is no generic
ResidueType and chemical information is sufficient, but generally unreadable to
PyRosetta. Presented here are two methods of introducing new (fullatom)
ResidueTypes, one temporary (loading in for the script or interpreter session)
the other permanent (appending to the database). Both processes start by
obtaining the proper .params files.

PyRosetta does not perform any of the initial changes required to improve the
accuracy or change the file format of a ligand. When docking a flexible
molecule, it is best to use multiple trials with each conformer separately.
PyRosetta cannot be used to generate conformers.

Procedural Outline

1) Obtain the ligand .mdl file
        -skip this step if the compound is present it PyRosetta (rare)
        -refine the chemical data (no PyRosetta tools for this)
        -convert the chemical data file to .mdl if necessary (try babel)
        -if necessary, generate conformers of the compound as separate files
2) Produce a .params file from the .mdl file and the script molfile_to_params.py
        -skip this step if the compound is present it PyRosetta (rare)
        -this will also yield a .pdb file which may be needed in step 3.

python molfile_to_params.py <MDL filename> -n <ResidueType name>

3) Produce the ligand-protein complex PDB file
        -obtain the desired protein PDB file (see pose_structure.py)
        -clean the PDB of other undesirable lines (HETATMs, waters, etc.)
        -manually insert the lines from the .pdb file produced in step 2. into
            the protein PDB file after all protein chains**
4) Check the ligand-PDB file to ensure:
        -the ligand residue column matches the ResidueType name used in step 2.
        -the ligand chain is named "X" (a convention)
        -the ligand chain occurs after all protein chains**
5) Load the ligand-protein complex PDB into PyRosetta by:
        >Temporarily creating a ResidueTypeSet (Method 1)
            -create a ResidueTypeSet using generate_nonstandard_residue_set,
                providing it a Vector1 of .params filenames
            -create an empty Pose object
            -load the PDB file data into the pose using pose_from_file providing
                the ResidueTypeSet as the second argument
        >Permanently modifying the chemical database (Method 2)
            -if using a new ligand/ResidueType:
                -place the new .params file into (in PyRosetta main directory)

/rosetta_database/chemical/residue_type_sets/fa_standard/residue_types

                -add the path to the new .params file to the file

/rosetta_database/chemical/residue_type_sets/fa_standard/residue_types.txt

            -if the ResidueType is present, but "turned off"
                -uncomment (or add) the path to the .params file in

/rosetta_database/chemical/residue_type_sets/fa_standard/residue_types.txt


=== Obtaining Ligand Data Files ===
Chemical formats are painfully unstandardized. Depending on your application and
resources, there are numerous options (not discussed here) for obtaining data
files for ligand compounds. When seeking chemical data files, numerous chemical
databases exist online (Pubchem etc.) as do tools for creating your compound.
The specific properties (partial charge, bond lengths and angles, etc.) may be
refined using other software*. Depending on the ligand, multiple conformers may
be necessary. Different software produce different chemical formats.
Conversion tools, such as babel or openbabel (openbabel.org), are required to
convert your file into the .mdl format for usage in the PyRosetta database. If
starting from an RCSB crystal structure, you can use PyMOL's "Save Molecule"
feature to produce an .mdl file of a ligand (the file extension appears as
".mol"). Molecular Networks (http://www.molecular-networks.com/) offers a free
online demo for chemical file format conversion
(http://www.molecular-networks.com/online_demos/convert_demo).

=== Converting to Params Files ===
An additional script (and other necessary scripts) are provided for converting
an .mdl file to a .params file (required for the PyRosetta database) and .pdb
files. Execute this script from the commandline providing the .mdl file as the
first argument and the ResidueType name as option "-n". For the .mdl file
provided with this script, the example commandline call would be:

>python molfile_to_params.py ATP.mdl -n ATP

This example will produce two files, "ATP.params" and "ATP_0001.pdb". The
.params file is necessary for the PyRosetta database, it defines the "ATP"
ResidueType. The .pdb file is produced for grafting the ligand into a PDB file
(the next step).

=== Preparing Ligand PDB Files ===
Now that the ResidueType is defined, the PDB file for ligand interface
prediction can be made. If the PDB file already has the ligand present, ensure
that its ResisueType column (PDB file format) is set to the ligand ResidueType
("ATP" for the example case). It is common practice to rename the chain to "X".
If the ligand is not already present in the PDB file, insert in manually
(using PyMOL, grep, awk, Python, Biopython, or whatever technique you prefer).
The script modfile_to_params.py provides a sample .pdb file for this purpose.
Ensure that the final PDB file has the proper ResidueType definition and chain
ID. As with DNA-protein PDB files, the ligand chain should be last**.

=== Loading a Ligand PDB File into PyRosetta ===

== Method 1: Temporarily using generate_nonstandard_residue_set ==
Inside the relevant script or interpreter, create a non-standard ResidueTypeSet
using the method generate_nonstandard_residue_set and use use pose_from_file to
load data into to a pose object. The method pose_from_file is overloaded
such that it can accept a Pose (poses), a ResidueTypeSet (residue_set), and a
string (filename) and load into the poses the data in the PDB file filename
using residue_set to define any unknown residues. This method is preferred when
the ligand is transitory (default). Permanently adding the ligand increases the
amount of data held at any time and may slow PyRosetta if too many are added.

== Method 2: Permanently by altering the PyRosetta database ==
Place (or copy) the new .params file somewhere in the PyRosetta fullatom
chemical database. Inside the PyRosetta main directory, place files within:

/minirosetta_database/chemical/residue_type_sets/fa_standard/residue_types

You must also add the path to the new ligand in the file:

/minirosetta_database/chemical/residue_type_sets/fa_standard/residue_types.txt

The database has many unused compounds. To activate these, simply uncomment the
necessary line in residue_types.txt.


When preparing PDB files for docking, remember that the two chains to dock must
be part of the same Pose object. This is easily attained by creating a PDB file
which includes both partners. If only interface structure prediction (high-
resolution) is used, the PDB file MUST contain the molecules ORIENTED properly
for the interface or the sampling will rarely find a proper structure.

Methods for downloading and generically "cleaning" PDB files should accompany
future PyRosetta releases.

*PyRosetta DOES NOT perform ANY of these refinements or predictions, simply
creating a molecule and introducing it to PyRosetta will rarely cause an error
although the results may be poor since the compound is inaccurately represented
**Otherwise the protein may be moved significantly by the protocol, these
methods were designed for usage with two-body docking problems and there is a
hard-coded definition of "upstream" and "downstream" for docking partners
The position of the upstream docking partner is held constant while the
downstream partner is altered by rigid-body perturbations, this does NOT affect
the accuracy of predictions but can be an annoyance since the protein
coordinates can significantly change though its conformation will not

"""

################
# A Real Example
"""
All of the default variables and parameters used above are specific to
the example with "test_lig.pdb" and "ATP.mdl", which is supposed to be simple,
straightforward, and speedy. Here is a more practical example:

Kemp elimination has been a targeted reaction for enzyme design using Rosetta.
Suppose you want to better understand the active site of these enzymes and
decide to investigate using PyRosetta.

1. Download a copy of RCSB PDB file 3NZ1 (remove waters and any other HETATM)
2. Extract the 5-Nitro-Benzotriazole substrate (preferably as a .mdl file)
        (note using PyMOL, you can save the molecule using the .mol extension)
3. Edit the PDB file removing waters, sulfate, and tartaric acid
4. Produce the .params file (see above) for 5-Nitro-Benzotriazole
        (listed as chain X resdidue 3NY in the PDB file), lets assume the
        substrate is saved as a .mol file named "3NY.mol"

>python molfile_to_params.py 3NY.mol -n 3NY

4. (optional) Test that the new PDB file is PyRosetta-friendly (it is)
5. Make a directory containing:
        -the PDB file for 3NZ1 (cleaned of waters and non-substrate HETATMs)
            lets name it "3NZ1.clean.pdb" here
        -this sample script (technically not required, but otherwise the
            commands in 6. would change since ligand_interface.py wouldn't
            be here)
6. Run the script from the commandline with appropriate arguments:

>python ligand_interface.py --pdb_filename 3NZ1.clean.pdb --partners A_X --ligand_params 3NY.params --jobs 400 --job_output 3NZ1_docking_output

        -The ligand .params file should be supplied using the temporary method
            (Method 1) described above since this script is setup to do this,
            the file "3NY.params" should have been successfully produced in step
            4., if you permanently add "3NY.params" to the chemical database,
            you do not need to supply anything for the "--ligand_params" option
        -The partners option, "A_X" is PDB specific, if you chose to retain
            different chains (in step 3.) or otherwise change the chain IDs
            in 3NZ1, make sure this string matches the desired chain interaction
        -400 trajectories is low, sampling docking conformations is difficult,
            typically thousands of (800-1000) trajectories are attempted
        -This script features the PyMOL_Observer (comment out to avoid using it),
            Monte Carlo simulations are not expected to produce kinetically
            meaningful results and as such, viewing the intermediates is only
            useful when understanding a protocol and rarely produces insight
            beyond the final output

7. Wait for output, this will take a while (performing 400 trajectories
        of the DockingHighRes is intensive)
8. Analyze the results (see INTERPRETING RESULTS above)

Note: this is a direct port of the Rosetta docking protocol and provides example
syntax for using this method within Python. This script also provides example
code for PyRosetta. A priori, a large number of jobs (~1000 or more) is required
to achieve useful results. The best protocols are somewhat protein-specific and
the scoring and sampling methods here may be customized to produce better
results on your complex.

"""

##############################
# Changing DNA Docking Sampling
"""
This script performs ligand-protein interface structure prediction but does NOT
perform global ligand-protein docking. Since there is no generic interface
detection, the input PDB file must have the ligand placed near the interface
that will be refined. If the DockMCMProtocol is applied to a pose
without placement near the interface, then the refinement may:
    -waste steps sampling the wrong interface
    -fail by predicting an incorrect interface very far from the true interface
    -fail by separating the ligand from the protein (usually due to a clash)
DockMCMProtocol does not require an independent randomization or perturbation
step to "seed" its prediction.

Additional refinement steps may increase the accuracy of the predicted
conformation (see refinement.py). Drastic moves (large conformational changes)
should be avoided; if they precede the protocol, the problems above may occur,
if they succeed the protocol, the protocol results may be lost. Please read
"Changing Docking Sampling" in docking.py and "Custom Docking" below to better
understand the explicit moves performed during docking if you wish to
create your own protocol.

Many other approaches to docking exist however PyRosetta exposes a myriad of
tools for constructing novel Monte Carlo algorithms for docking prediction.

Please try alternate sampling methods to better understand how these
algorithms perform and to find what moves best suite your problem.

"""

#############################
# Changing DNA Docking Scoring
"""
The "dna" ScoreFunction is optimized for DNA-protein interface prediction and
is very similar to the "standard" ScoreFunction with modified weights. The
score term "fa_elec" is activated to (further) penalize structures with poor
electrostatic interactions since many DNA-binding proteins have a large number
of positively charged residues to bind the negatively charged phosphate
backbone of DNA.

The dominant interactions in DNA-protein binding are not well studied. Many
score terms in Rosetta may improve the accuracy of DNA-protein interface
structure prediction. Please try alternate scoring functions or unique
selection methods to better understand which scoring terms contribute to
performance and to find what scoring best suites your problem.

"""

###############
# Other Docking
"""
Binding interaction drive MANY biological processes. While protein-protein
docking addresses many possibilities, there are other compounds which a user
may wish to investigate. Rosetta (and thus the core methods exposed in
PyRosetta) provides protocols for predicting docking with other chemical
entities such as DNA, RNA (see dna_docking.py), and small ligands (see
ligand_docking.py). Rosetta provides symmetry tools for predicting symmetric
protein complexes (another kind of protein-protein interaction) but these
methods are not currently supported in PyRosetta. Peptide (or other flexible
molecule) docking does not currently have a fine tuned protocol however
PyRosetta provides all the tools necessary for such algorithms. When binding
a highly flexible molecule, the sampling space becomes much larger,
compounding the already large search space of docking and conformation
prediction. Such algorithms are anticipated to require extremely intensive
sampling to achieve useful results. Antibody docking is supported by Rosetta
is likewise confounded by a very large search space. Small molecule docking
covers a large variety of biologically relevant interactions including
(potentially) the placement of structurally important water molecules.

Problem                    Rosetta tool        In PyRosetta?
protein-protein docking    DockingProtocol     yes
small molecule docking     LigandDockProtocol  components, use DockMCMProtocol
nucleic acid interface     DNAInterfaceFinder  components, use DockMCMProtocol
symmetric complexes        SymDockProtocol     no
flexible polymer docking   -                   no
antibody docking           AntibodyModeler     no
water placement            -                   tools, no direct protocol

"""