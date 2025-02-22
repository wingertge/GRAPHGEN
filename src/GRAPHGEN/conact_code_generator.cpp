// Copyright (c) 2020, the GRAPHGEN contributors, as
// shown by the AUTHORS file. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "conact_code_generator.h"

#include "utilities.h"
#include <cstddef>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

using namespace std;

// Generate string to access a pixel value using pointers
string GenerateAccessPixelCode(const string &img_name, const pixel &p) {
  string slice_id = "";
  if (p.coords_.size() > 2) {
    slice_id = "slice" + string(p.coords_[2] < 0 ? "1" : "0") +
               to_string(abs(p.coords_[2])) + "_";
  }

  string row_id = "row" + string(p.coords_[1] < 0 ? "1" : "0") +
                  to_string(abs(p.coords_[1]));
  string col = "";
  if (p.coords_[0] > 0) {
    col += " + " + to_string(abs(p.coords_[0]));
  } else if (p.coords_[0] < 0) {
    col += " - " + to_string(abs(p.coords_[0]));
  }

  return "*" + img_name + slice_id + row_id + ".add((c" + col + ") as usize)";
}

string CreateAssignmentCodeRec(const std::vector<std::string> &pixels_names,
                               const pixel_set &names) {
  if (pixels_names.size() == 1) {
    pixel p = names[pixels_names.front()];
    return GenerateAccessPixelCode("img_labels_", p);
  }

  std::vector<std::string> pixels_names_seta(pixels_names.size() / 2),
      pixels_names_setb(pixels_names.size() - (pixels_names.size() / 2));

  std::copy_n(pixels_names.begin(), pixels_names_seta.size(),
              pixels_names_seta.begin());
  std::copy_n(pixels_names.begin() + pixels_names_seta.size(),
              pixels_names_setb.size(), pixels_names_setb.begin());

  return "LabelsSolver::merge(" +
         CreateAssignmentCodeRec(pixels_names_setb, names) + ", " +
         CreateAssignmentCodeRec(pixels_names_seta, names) + ", solver)";
}

string CreateAssignmentCode(const string &action, const pixel_set &names) {
  if (action == "nothing") {
    return "0.elem()";
  }

  string action_ = action.substr(3);
  if (action_ == "newlabel") {
    return "solver.new_label()";
  }

  std::vector<std::string> pixels_names;
  StringSplit(action_, pixels_names);

  assert(pixels_names.size() > 0 && "Something wrong with actions");

  return CreateAssignmentCodeRec(pixels_names, names);
}

// This is the version for CTBE algorithm, still very raw
string CreateActionCodeCtbe(const string &action, const pixel_set &names,
                            const string &assignment_variable) {
  if (action == "nothing") {
    return "NOTHING";
  }

  string action_ = action.substr(3);
  if (action_ == "newlabel") {
    return assignment_variable + " solver.new_label()";
  }

  std::vector<std::string> pixels_names;
  StringSplit(action_, pixels_names);

  assert(pixels_names.size() > 0 && "Something wrong with actions");

  return assignment_variable + " " +
         CreateAssignmentCodeRec(pixels_names, names);
}

void GeneratePointersCode(ofstream &os, const rule_set &rs) {
  // The names of pointers are identified by the following string
  //	<image_name>_<slice_identifier>_<row_identifier>
  //
  // slice identifiers can be (first number is the sign):
  //	- 'slice00' for the current slice (z)
  //	- 'slice11' for the slice z - 1
  //	- 'slice12' for the slice z - 2
  //  - 'slice01' for the slice z + 1
  //  - 'slice02' for the slice z + 2
  //  - .. and so on
  //
  // row identifiers can be (first number is the sign):
  //	- 'row00' for the current row (y)
  //	- 'row11' for the row y - 1
  //	- 'row12' for the row y - 2
  //  - 'row01' for the row y + 1
  //  - 'row02' for the row y + 2
  //  - .. and so on

  // Pointers:
  os << "//Pointers:\n";

  auto &shifts = rs.ps_.shifts_; // Shifts on each dim -> [x, y] or [x, y, z]
  size_t n_dims = shifts.size(); // Here we get how many dims image has

  stringstream global_ss, in_ss, out_ss;
  string type_in_prefix_string = "let ";
  string type_out_prefix_string = "let ";

  // TODO: 3D generation only works with unitary shift
  // x is always ignored because we always create row pointers
  switch (n_dims) {
  case 2: {
    string base_row_in_name = "img_row00";
    string base_row_out_name = "img_labels_row00";
    string base_row_in = type_in_prefix_string + base_row_in_name +
                         " = img.add(r * w as usize);";
    string base_row_out = type_out_prefix_string + base_row_out_name +
                          " = img_labels.add(r * w as usize);";

    in_ss << "// Row pointers for the input image \n";
    in_ss << base_row_in << "\n";

    out_ss << "// Row pointers for the output image \n";
    out_ss << base_row_out << "\n";

    for (int j = -shifts[1]; j < shifts[1];
         ++j) { // TODO: should use min and max y in mask

      if (j == 0) {
        continue;
      }

      auto modifier = j < 0 ? to_string(j) : "+ " + to_string(j);

      string complete_string_in =
          type_in_prefix_string + "img_row" + to_string(j < 0) +
          to_string(abs(j)) + " = img.add((r " + modifier + ") * w as usize);";
      in_ss << complete_string_in + "\n";

      string complete_string_out = type_out_prefix_string + "img_labels_row" +
                                   to_string(j < 0) + to_string(abs(j)) +
                                   " = img_labels.add((r " + modifier +
                                   ") * w as usize);";
      out_ss << complete_string_out + "\n";
    }

    break;
  }
  case 3: {
    // TODO: this generation only works with unitary shift

    // Current slice
    string base_row_in_name = "img_slice00_row00";
    string base_row_out_name = "img_labels_slice00_row00";
    string base_row_in = type_in_prefix_string + base_row_in_name +
                         " = img_.ptr<unsigned char>(s, r);";
    string base_row_out = type_out_prefix_string + base_row_out_name +
                          " = img_labels_.ptr<unsigned>(s, r);";

    in_ss << "// Row pointers for the input image (current slice) \n";
    in_ss << base_row_in + "\n";

    out_ss << "// Row pointers for the output image (current slice)\n";
    out_ss << base_row_out + "\n";

    for (int j = -shifts[1]; j < shifts[1]; ++j) {

      if (j == 0) {
        continue;
      }

      string complete_string_in =
          type_in_prefix_string + "img_slice00_row" + to_string(j < 0) +
          to_string(abs(j)) + " = (unsigned char *)(((char *)" +
          base_row_in_name + ") + img_.step.p[1] * " + to_string(j) + ");";
      in_ss << complete_string_in + "\n";

      string complete_string_out =
          type_out_prefix_string + "img_labels_slice00_row" + to_string(j < 0) +
          to_string(abs(j)) + " = (unsigned *)(((char *)" + base_row_out_name +
          ") + img_labels_.step.p[1] * " + to_string(j) + ");";
      out_ss << complete_string_out + "\n";
    }

    in_ss << "\n// Row pointers for the input image (previous slice) \n";
    out_ss << "\n// Row pointers for the output image (previous slice)\n";

    // Previous slice
    base_row_in = type_in_prefix_string + base_row_in_name +
                  " = img_.ptr<unsigned char>(s, r);";
    base_row_out = type_out_prefix_string + base_row_out_name +
                   " = img_labels_.ptr<unsigned>(s, r);";

    for (int j = -shifts[1]; j <= shifts[1]; ++j) {

      string complete_string_in =
          type_in_prefix_string + "img_slice11_row" + to_string(j < 0) +
          to_string(abs(j)) + " = (unsigned char *)(((char *)" +
          base_row_in_name + ") - img_.step.p[0] + img_.step.p[1] * " +
          to_string(j) + ");";
      in_ss << complete_string_in + "\n";

      string complete_string_out =
          type_out_prefix_string + "img_labels_slice11_row" + to_string(j < 0) +
          to_string(abs(j)) + " = (unsigned *)(((char *)" + base_row_out_name +
          ") - img_labels_.step.p[0] + img_labels_.step.p[1] * " +
          to_string(j) + ");";
      out_ss << complete_string_out + "\n";
    }
  } break;
  }
  global_ss << in_ss.str() + "\n" + out_ss.str();
  os << global_ss.str();
}

std::map<string, string> GenerateConditionsCode(const rule_set &rs,
                                                bool with_conditions) {
  auto result = std::map<string, string>();
  auto &shifts = rs.ps_.shifts_; // Shifts on each dim -> [x, y] or [x, y, z]
  size_t n_dims = shifts.size(); // Here we get how many dims image has

  vector<string> counters_names = {"c", "r", "s"};
  vector<string> sizes_names = {"w", "h", "d"};

  for (const auto &p : rs.ps_) {
    auto condition_code = std::ostringstream();

    // stringstream col;
    if (with_conditions) {
      for (size_t i = 0; i < n_dims; ++i) {
        if (p.coords_[i] < 0) {
          condition_code << counters_names[i] << " > " << -p.coords_[i] - 1
                         << " && ";
        } else if (p.coords_[i] > 0) {
          condition_code << counters_names[i] << " < " << sizes_names[i]
                         << " - " << p.coords_[i] << " && ";
        }
      }
    }

    condition_code << "(" << GenerateAccessPixelCode("img_", p)
                   << ").to_u8() > 0";

    result[p.name_] = condition_code.str();
  }

  return result;
}

// This is the function to generate Connected Component Labeling action code
std::map<size_t, string> GenerateActionsCode(const rule_set &rs,
                                             const pixel_set &names,
                                             bool with_continues = true) {
  auto &shifts = rs.ps_.shifts_; // Shifts on each dim -> [x, y] or [x, y, z]
  size_t n_dims = shifts.size(); // Here we get how many dims image has
  auto result = std::map<size_t, string>();

  // Actions:
  for (size_t a = 0; a < rs.actions.size(); ++a) {
    auto action = ostringstream();

    string cur_action = rs.actions[a];

    string where_to_write = "*img_labels_" +
                            string(n_dims > 2 ? "slice00_" : "") +
                            "row00.add(c as usize) = ";

    action << where_to_write << CreateAssignmentCode(cur_action, names) << ";";

    if (with_continues) {
      action << "return None;";
    }

    result[a + 1] = action.str();
  }

  return result;
}

// This is the function to generate Thinning action code
void GenerateThinningActionsCode(ofstream &os, const rule_set &rs,
                                 bool with_continues = true) {
  // Actions:
  os << "\n\n//Actions:\n";
  for (size_t a = 0; a < rs.actions.size(); ++a) {
    const auto &action = rs.actions[a];

    os << "#define ACTION_" + std::to_string(a + 1);

    if (action == "keep0") {
      os << " ; // keep0\n";
    }

    if (action == "keep1") {
      os << " img_row00[c] = 1; // keep1\n";
    }

    if (action == "change0") {
      os << " modified = true; // change0\n";
    }
  }
}

// This is the function to generate Chain Code action code
void GenerateChaincodeActionsCode(ofstream &os, const rule_set &rs,
                                  bool with_continues = true) {
  // Actions:
  os << "\n\n//Actions:\n";
  for (size_t a = 0; a < rs.actions.size(); ++a) {
    const auto &action = rs.actions[a];

    os << "#define ACTION_" << (a + 1) << "\tpos = ProcessPixel<" << action
       << "\t>(r, c, rccode, chains, pos);";

    if (with_continues) {
      os << " continue;";
    }

    os << "\n";
  }
}

bool GeneratePointersConditionsActionsCode(
    const rule_set &rs, GenerateConditionActionCodeFlags flag,
    GenerateActionCodeTypes type, std::optional<pixel_set> names) {
  bool actions_with_conditions =
      flag & GenerateConditionActionCodeFlags::CONDITIONS_WITH_IFS;
  bool actions_with_continue =
      flag & GenerateConditionActionCodeFlags::ACTIONS_WITH_CONTINUE;

  ofstream os(conf.code_path_);
  if (!os) {
    return false;
  }

  if (!names) {
    names = rs.ps_;
  }

  GeneratePointersCode(os, rs);

  switch (type) {
  case GenerateActionCodeTypes::LABELING:
    // GenerateActionsCode(os, rs, names.value(), actions_with_continue);
    break;
  case GenerateActionCodeTypes::THINNING:
    GenerateThinningActionsCode(os, rs, actions_with_continue);
    break;
  case GenerateActionCodeTypes::CHAIN_CODE:
    GenerateChaincodeActionsCode(os, rs, actions_with_continue);
    break;
  default:
    std::cout << "WARNING: the specified algorithms type is not valid. The "
                 "ACTION code won't be generated\n";
  }

  return true;
}

std::map<std::string, std::string>
GenerateConditions(const rule_set &rs, GenerateConditionActionCodeFlags flag) {
  bool actions_with_conditions =
      flag & GenerateConditionActionCodeFlags::CONDITIONS_WITH_IFS;

  return GenerateConditionsCode(rs, actions_with_conditions);
}

std::map<size_t, std::string>
GenerateActions(const rule_set &rs, GenerateConditionActionCodeFlags flag,
                GenerateActionCodeTypes type, std::optional<pixel_set> names) {
  bool actions_with_continue =
      flag & GenerateConditionActionCodeFlags::ACTIONS_WITH_CONTINUE;

  if (!names) {
    names = rs.ps_;
  }

  switch (type) {
  case GenerateActionCodeTypes::LABELING:
    return GenerateActionsCode(rs, names.value(), actions_with_continue);
  case GenerateActionCodeTypes::THINNING:
    // GenerateThinningActionsCode(rs, actions_with_continue);
    return std::map<size_t, std::string>();
  case GenerateActionCodeTypes::CHAIN_CODE:
    // GenerateChaincodeActionsCode(rs, actions_with_continue);
    return std::map<size_t, std::string>();
  default:
    std::cout << "WARNING: the specified algorithms type is not valid. The "
                 "ACTION code won't be generated\n";
  }
}

// bool GenerateActionsForCtbe(const string& filename, const rule_set& rs) {
//     ofstream os(filename);
//
//     if (!os) {
//         return false;
//     }
//
//     // Actions:
//     os << "\n\n//Actions:\n";
//     os << "#define NOTHING \n";
//     for (size_t a = 0; a < rs.actions.size(); ++a) {
//
//         string cur_action = rs.actions[a];
//
//         os << "// Action " << a + 1 << ": " << cur_action << "\n";
//         os << "#define ACTION_" << a + 1 << " ";
//
//         vector<string> pixels_actions;
//         StringSplit(cur_action, pixels_actions, ',');
//
//         if (pixels_actions[1].substr(3) == "e" &&
//         (pixels_actions[2].substr(3) == "e" || pixels_actions[2].substr(3) ==
//         "g")) {
//             os << CreateActionCodeCtbe(pixels_actions[0], rs,
//             "img_labels_row00[c] = img_labels_row01[c] = img_labels_row02[c]
//             = ") << "; continue; \n";
//         }
//         else {
//             if (pixels_actions[1].substr(3) == "e") {
//                 os << CreateActionCodeCtbe(pixels_actions[0], rs,
//                 "img_labels_row00[c] = img_labels_row01[c] = ") << "; "; os
//                 << CreateActionCodeCtbe(pixels_actions[2], rs,
//                 "img_labels_row02[c] = ") << "; continue;\n";
//             }
//             else {
//                 if (pixels_actions[2].substr(3) == "e") {
//                     os << CreateActionCodeCtbe(pixels_actions[0], rs,
//                     "img_labels_row00[c] = img_labels_row02[c] = ") << "; ";
//                     os << CreateActionCodeCtbe(pixels_actions[1], rs,
//                     "img_labels_row01[c] = ") << "; continue;\n";
//                 }
//                 else {
//                     // Il pixel "e" ha azione unica
//
//                     os << CreateActionCodeCtbe(pixels_actions[0], rs,
//                     "img_labels_row00[c] = ") << "; "; if
//                     (pixels_actions[2].substr(3) == "e") {
//                         os << CreateActionCodeCtbe(pixels_actions[1], rs,
//                         "img_labels_row01[c] = img_labels_row02[c] = ") << ";
//                         continue;\n";
//                     }
//                     else {
//                         // Ogni pixel ha la sua azione
//                         os << CreateActionCodeCtbe(pixels_actions[1], rs,
//                         "img_labels_row01[c] = ") << "; "; os <<
//                         CreateActionCodeCtbe(pixels_actions[2], rs,
//                         "img_labels_row02[c] = ") << "; continue;\n";
//                     }
//                 }
//             }
//         }
//     }
//
//     return true;
// }
