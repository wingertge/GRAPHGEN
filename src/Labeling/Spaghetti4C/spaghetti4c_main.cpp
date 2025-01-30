// Copyright (c) 2020, the GRAPHGEN contributors, as
// shown by the AUTHORS file. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "conact_code_generator.h"
#include "rosenfeld_ruleset.h"

using namespace std;

int main() {
  string algorithm_name = "Spaghetti4C";

  string mask_name = "Grana";

  conf = ConfigData(algorithm_name, mask_name);

  Rosenfeld4CRS g_rs;
  auto rs = g_rs.GetRuleSet();

  // Call GRAPHGEN:
  // 1) Load or generate Optimal Decision Tree based on Grana mask
  BinaryDrag<conact> bd = GetOdt(rs);

  // 2) Draw the generated tree to pdf
  string tree_filename = algorithm_name + "_tree";
  DrawDagOnFile(tree_filename, bd);

  // 3) Generate forests of trees
  LOG(algorithm_name + " - making forests", ForestHandler fh(bd, rs.ps_););

  // 5) Compress the forests
  fh.Compress(DragCompressorFlags::PRINT_STATUS_BAR |
              DragCompressorFlags::IGNORE_LEAVES);

  // 6) Draw the compressed forests on file
  fh.DrawOnFile(algorithm_name, DrawDagFlags::DELETE_DOTCODE);

  GeneratePointersConditionsActionsCode(rs,
                                        GenerateConditionActionCodeFlags::NONE,
                                        GenerateActionCodeTypes::LABELING);
  auto conditions =
      GenerateConditions(rs, GenerateConditionActionCodeFlags::NONE);
  auto actions = GenerateActions(rs, GenerateConditionActionCodeFlags::NONE,
                                 GenerateActionCodeTypes::LABELING);

  // 7) Generate the C/C++ code taking care of the names used
  //    in the Grana's rule set GranaRS
  fh.GenerateCode(conditions, actions, BeforeMainNoEndTree);

  return EXIT_SUCCESS;
}