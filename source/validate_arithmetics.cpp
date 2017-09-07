// Copyright (c) 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Ensures type declarations are unique unless allowed by the specification.

#include "validate.h"

#include "diagnostic.h"
#include "opcode.h"
#include "val/instruction.h"
#include "val/validation_state.h"

namespace libspirv {

namespace {

// Returns operand word for given instruction and operand index.
// The operand is expected to only have one word.
inline uint32_t GetOperandWord(const spv_parsed_instruction_t* inst,
                               size_t operand_index) {
  assert(operand_index < inst->num_operands);
  const spv_parsed_operand_t& operand = inst->operands[operand_index];
  assert(operand.num_words == 1);
  return inst->words[operand.offset];
}

// Returns the type id of instruction operand at |operand_index|.
// The operand is expected to be an id.
inline uint32_t GetOperandTypeId(ValidationState_t& _,
                               const spv_parsed_instruction_t* inst,
                               size_t operand_index) {
  return _.GetTypeId(GetOperandWord(inst, operand_index));
}

}

// Validates correctness of arithmetic instructions.
spv_result_t ArithmeticsPass(ValidationState_t& _,
                            const spv_parsed_instruction_t* inst) {
  const SpvOp opcode = static_cast<SpvOp>(inst->opcode);
  const uint32_t result_type = inst->type_id;

  switch (opcode) {
    case SpvOpFAdd:
    case SpvOpFSub:
    case SpvOpFMul:
    case SpvOpFDiv:
    case SpvOpFRem:
    case SpvOpFMod:
    case SpvOpFNegate: {
      if (!_.IsFloatScalarType(result_type) &&
          !_.IsFloatVectorType(result_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected floating scalar or vector type as Result Type: "
            << spvOpcodeString(opcode);

      for (size_t operand_index = 2; operand_index < inst->num_operands;
           ++operand_index) {
        if (GetOperandTypeId(_, inst, operand_index) != result_type)
          return _.diag(SPV_ERROR_INVALID_DATA)
              << "Expected arithmetic operands to be of Result Type: "
              << spvOpcodeString(opcode) << " operand index " << operand_index;
      }
      break;
    }

    case SpvOpUDiv:
    case SpvOpUMod: {
      if (!_.IsUnsignedIntScalarType(result_type) &&
          !_.IsUnsignedIntVectorType(result_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected unsigned int scalar or vector type as Result Type: "
            << spvOpcodeString(opcode);

      for (size_t operand_index = 2; operand_index < inst->num_operands;
           ++operand_index) {
        if (GetOperandTypeId(_, inst, operand_index) != result_type)
          return _.diag(SPV_ERROR_INVALID_DATA)
              << "Expected arithmetic operands to be of Result Type: "
              << spvOpcodeString(opcode) << " operand index " << operand_index;
      }
      break;
    }

    case SpvOpISub:
    case SpvOpIAdd:
    case SpvOpIMul:
    case SpvOpSDiv:
    case SpvOpSMod:
    case SpvOpSRem:
    case SpvOpSNegate: {
      if (!_.IsIntScalarType(result_type) &&
          !_.IsIntVectorType(result_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected int scalar or vector type as Result Type: "
            << spvOpcodeString(opcode);

      const uint32_t dimension = _.GetDimension(result_type);
      const uint32_t bit_width = _.GetBitWidth(result_type);

      for (size_t operand_index = 2; operand_index < inst->num_operands;
           ++operand_index) {

        const uint32_t type_id = GetOperandTypeId(_, inst, operand_index);
        if (!type_id ||
            (!_.IsIntScalarType(type_id) && !_.IsIntVectorType(type_id)))
          return _.diag(SPV_ERROR_INVALID_DATA)
              << "Expected int scalar or vector type as operand: "
              << spvOpcodeString(opcode) << " operand index " << operand_index;

        if (_.GetDimension(type_id) != dimension)
          return _.diag(SPV_ERROR_INVALID_DATA)
              << "Expected arithmetic operands to have the same dimension "
              << "as Result Type: "
              << spvOpcodeString(opcode) << " operand index " << operand_index;

        if (_.GetBitWidth(type_id) != bit_width)
          return _.diag(SPV_ERROR_INVALID_DATA)
              << "Expected arithmetic operands to have the same bit width "
              << "as Result Type: "
              << spvOpcodeString(opcode) << " operand index " << operand_index;
      }
      break;
    }

    case SpvOpDot: {
      if (!_.IsFloatScalarType(result_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected float scalar type as Result Type: "
            << spvOpcodeString(opcode);

      uint32_t first_vector_num_components = 0;

      for (size_t operand_index = 2; operand_index < inst->num_operands;
           ++operand_index) {
        const uint32_t type_id = GetOperandTypeId(_, inst, operand_index);

        if (!type_id || !_.IsFloatVectorType(type_id))
          return _.diag(SPV_ERROR_INVALID_DATA)
              << "Expected float vector as operand: "
              << spvOpcodeString(opcode) << " operand index " << operand_index;


        const uint32_t component_type = _.GetComponentType(type_id);
        if (component_type != result_type)
          return _.diag(SPV_ERROR_INVALID_DATA)
              << "Expected component type to be equal to Result Type: "
              << spvOpcodeString(opcode) << " operand index " << operand_index;

        const uint32_t num_components = _.GetDimension(type_id);
        if (operand_index == 2) {
          first_vector_num_components = num_components;
        } else if (num_components != first_vector_num_components) {
          return _.diag(SPV_ERROR_INVALID_DATA)
              << "Expected operands to have the same number of componenets: "
              << spvOpcodeString(opcode);
        }
      }
      break;
    }

    case SpvOpVectorTimesScalar: {
      if (!_.IsFloatVectorType(result_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected float vector type as Result Type: "
            << spvOpcodeString(opcode);

      const uint32_t vector_type_id = GetOperandTypeId(_, inst, 2);
      if (result_type != vector_type_id)
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected vector operand type to be equal to Result Type: "
            << spvOpcodeString(opcode);

      const uint32_t component_type = _.GetComponentType(vector_type_id);

      const uint32_t scalar_type_id = GetOperandTypeId(_, inst, 3);
      if (component_type != scalar_type_id)
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected scalar operand type to be equal to the component "
            << "type of the vector operand: "
            << spvOpcodeString(opcode);

      break;
    }

    case SpvOpMatrixTimesScalar: {
      if (!_.IsFloatMatrixType(result_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected float matrix type as Result Type: "
            << spvOpcodeString(opcode);

      const uint32_t matrix_type_id = GetOperandTypeId(_, inst, 2);
      if (result_type != matrix_type_id)
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected matrix operand type to be equal to Result Type: "
            << spvOpcodeString(opcode);

      const uint32_t component_type = _.GetComponentType(matrix_type_id);

      const uint32_t scalar_type_id = GetOperandTypeId(_, inst, 3);
      if (component_type != scalar_type_id)
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected scalar operand type to be equal to the component "
            << "type of the matrix operand: "
            << spvOpcodeString(opcode);

      break;
    }

    case SpvOpVectorTimesMatrix: {
      const uint32_t vector_type_id = GetOperandTypeId(_, inst, 2);
      const uint32_t matrix_type_id = GetOperandTypeId(_, inst, 3);

      if (!_.IsFloatVectorType(result_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected float vector type as Result Type: "
            << spvOpcodeString(opcode);

      const uint32_t res_component_type = _.GetComponentType(result_type);

      if (!vector_type_id || !_.IsFloatVectorType(vector_type_id))
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected float vector type as left operand: "
            << spvOpcodeString(opcode);

      if (res_component_type != _.GetComponentType(vector_type_id))
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected component types of Result Type and vector to be "
            << "equal: " << spvOpcodeString(opcode);

      uint32_t matrix_num_rows = 0;
      uint32_t matrix_num_cols = 0;
      uint32_t matrix_col_type = 0;
      uint32_t matrix_component_type = 0;
      if (!_.GetMatrixTypeInfo(matrix_type_id, &matrix_num_rows,
                               &matrix_num_cols, &matrix_col_type,
                               &matrix_component_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected float matrix type as right operand: "
            << spvOpcodeString(opcode);

      if (res_component_type != matrix_component_type)
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected component types of Result Type and matrix to be "
            << "equal: " << spvOpcodeString(opcode);

      if (matrix_num_cols != _.GetDimension(result_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected number of columns of the matrix to be equal to "
            << "Result Type vector size: " << spvOpcodeString(opcode);

      if (matrix_num_rows != _.GetDimension(vector_type_id))
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected number of rows of the matrix to be equal to the "
            << "vector operand size: " << spvOpcodeString(opcode);

      break;
    }

    case SpvOpMatrixTimesVector: {
      const uint32_t matrix_type_id = GetOperandTypeId(_, inst, 2);
      const uint32_t vector_type_id = GetOperandTypeId(_, inst, 3);

      if (!_.IsFloatVectorType(result_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected float vector type as Result Type: "
            << spvOpcodeString(opcode);

      uint32_t matrix_num_rows = 0;
      uint32_t matrix_num_cols = 0;
      uint32_t matrix_col_type = 0;
      uint32_t matrix_component_type = 0;
      if (!_.GetMatrixTypeInfo(matrix_type_id, &matrix_num_rows,
                               &matrix_num_cols, &matrix_col_type,
                               &matrix_component_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected float matrix type as left operand: "
            << spvOpcodeString(opcode);

      if (result_type != matrix_col_type)
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected column type of the matrix to be equal to Result Type: "
            << spvOpcodeString(opcode);

      if (!vector_type_id || !_.IsFloatVectorType(vector_type_id))
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected float vector type as right operand: "
            << spvOpcodeString(opcode);

      if (matrix_component_type != _.GetComponentType(vector_type_id))
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected component types of the operands to be equal: "
            << spvOpcodeString(opcode);

      if (matrix_num_cols != _.GetDimension(vector_type_id))
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected number of columns of the matrix to be equal to the "
            << "vector size: " << spvOpcodeString(opcode);

      break;
    }

    case SpvOpMatrixTimesMatrix: {
      const uint32_t left_type_id = GetOperandTypeId(_, inst, 2);
      const uint32_t right_type_id = GetOperandTypeId(_, inst, 3);

      uint32_t res_num_rows = 0;
      uint32_t res_num_cols = 0;
      uint32_t res_col_type = 0;
      uint32_t res_component_type = 0;
      if (!_.GetMatrixTypeInfo(result_type, &res_num_rows, &res_num_cols,
                               &res_col_type, &res_component_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected float matrix type as Result Type: "
            << spvOpcodeString(opcode);

      uint32_t left_num_rows = 0;
      uint32_t left_num_cols = 0;
      uint32_t left_col_type = 0;
      uint32_t left_component_type = 0;
      if (!_.GetMatrixTypeInfo(left_type_id, &left_num_rows, &left_num_cols,
                               &left_col_type, &left_component_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected float matrix type as left operand: "
            << spvOpcodeString(opcode);

      uint32_t right_num_rows = 0;
      uint32_t right_num_cols = 0;
      uint32_t right_col_type = 0;
      uint32_t right_component_type = 0;
      if (!_.GetMatrixTypeInfo(right_type_id, &right_num_rows, &right_num_cols,
                               &right_col_type, &right_component_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected float matrix type as right operand: "
            << spvOpcodeString(opcode);

      if (!_.IsFloatScalarType(res_component_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected float matrix type as Result Type: "
            << spvOpcodeString(opcode);

      if (res_col_type != left_col_type)
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected column types of Result Type and left matrix to be "
            << "equal: " << spvOpcodeString(opcode);

      if (res_component_type != right_component_type)
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected component types of Result Type and right matrix to be "
            << "equal: " << spvOpcodeString(opcode);

      if (res_num_cols != right_num_cols)
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected number of columns of Result Type and right matrix to "
            << "be equal: " << spvOpcodeString(opcode);

      if (left_num_cols != right_num_rows)
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected number of columns of left matrix and number of rows "
            << "of right matrix to be equal: " << spvOpcodeString(opcode);

      assert(left_num_rows == res_num_rows);
      break;
    }

    case SpvOpOuterProduct: {
      const uint32_t left_type_id = GetOperandTypeId(_, inst, 2);
      const uint32_t right_type_id = GetOperandTypeId(_, inst, 3);

      uint32_t res_num_rows = 0;
      uint32_t res_num_cols = 0;
      uint32_t res_col_type = 0;
      uint32_t res_component_type = 0;
      if (!_.GetMatrixTypeInfo(result_type, &res_num_rows, &res_num_cols,
                               &res_col_type, &res_component_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected float matrix type as Result Type: "
            << spvOpcodeString(opcode);

      if (left_type_id != res_col_type)
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected column type of Result Type to be equal to the type "
            << "of the left operand: "
            << spvOpcodeString(opcode);

      if (!right_type_id || !_.IsFloatVectorType(right_type_id))
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected float vector type as right operand: "
            << spvOpcodeString(opcode);

      if (res_component_type != _.GetComponentType(right_type_id))
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected component types of the operands to be equal: "
            << spvOpcodeString(opcode);

      if (res_num_cols != _.GetDimension(right_type_id))
        return _.diag(SPV_ERROR_INVALID_DATA)
            << "Expected number of columns of the matrix to be equal to the "
            << "vector size of the right operand: " << spvOpcodeString(opcode);

      break;
    }

    // TODO(atgoo@github.com): Support other operations.

    default:
      break;
  }

  return SPV_SUCCESS;
}

}  // namespace libspirv