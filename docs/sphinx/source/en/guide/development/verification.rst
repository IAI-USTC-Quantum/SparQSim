Testing and Verification
========================

These practices fit into step 4 of the :doc:`development workflow <workflow>`.

Verification Checklist
----------------------

- [ ] State vector verification: compare amplitudes against known results
- [ ] Fidelity tests: compare against a reference implementation
- [ ] Noise sensitivity analysis
- [ ] Performance benchmarking

State Vector Verification
-------------------------

Compare the state vector produced by your implementation against theoretical values or a reference implementation:

.. code-block:: cpp

   // Extract amplitudes
   double success_prob = 0;
   std::vector<complex_t> full_amplitudes(full_size);
   for (auto& s : state) {
       full_amplitudes[s.get(addr_reg).value] = s.amplitude;
   }

   // Compare against the target state amplitudes
   for (auto target_position : target_positions) {
       success_prob += abs_sqr(full_amplitudes[target_position]);
   }

   fmt::print("Success probability: {}\n", success_prob);

Fidelity Testing
----------------

.. code-block:: cpp

   double fidelity = get_fidelity();  // use the built-in fidelity computation
   fmt::print("Fidelity: {}\n", fidelity);

Noise Sensitivity Analysis
--------------------------

.. code-block:: cpp

   struct GroverTestArguments {
       double depolarizing = 0.0;
       double damping = 0.0;

       std::map<OperationType, double> generate_noise() const {
           std::map<OperationType, double> noise;
           if (depolarizing > 0.0)
               noise[OperationType::Depolarizing] = depolarizing;
           if (damping > 0.0)
               noise[OperationType::Damping] = damping;
           return noise;
       }
   };

Performance Benchmarking
------------------------

.. code-block:: cpp

   // Use the profiler to track performance
   {
       profiler _("MyAlgorithm");
       MyAlgorithm(params)(state);
   }

   fmt::print("{}\n", profiler::get_all_profiles_v2());

Command-Line Argument Parsing Template
--------------------------------------

.. code-block:: cpp

   inline MyArgs parse_arguments(int argc, const char** argv) {
       argparse::ArgumentParser parser("MyAlgorithm", "Description");
       parser.add_argument()
           .names({ "-q", "--qubit" })
           .description("qubit number")
           .required(false);

       parser.enable_help();
       auto err = parser.parse(argc, argv);
       if (err || parser.exists("help")) {
           parser.print_help();
           return {};
       }

       MyArgs args;
       if (parser.exists("qubit"))
           args.qubit = parser.get<int>("qubit");

       return args;
   }
