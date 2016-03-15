/* Copyright 2015 Codethink Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <math.h>

#include "ofc/sema.h"

static void ofc_sema_stmt_io_write__cleanup(
	ofc_sema_stmt_t s)
{
	ofc_sema_expr_delete(s.io_write.unit);
	ofc_sema_expr_delete(s.io_write.format);
	ofc_sema_expr_delete(s.io_write.advance);
	ofc_sema_expr_delete(s.io_write.err);
	ofc_sema_expr_delete(s.io_write.iostat);
	ofc_sema_expr_delete(s.io_write.rec);
	ofc_sema_expr_list_delete(s.io_write.iolist);
}

ofc_sema_stmt_t* ofc_sema_stmt_io_write(
	ofc_sema_scope_t* scope,
	const ofc_parse_stmt_t* stmt)
{
	if (!scope || !stmt
		|| (stmt->type != OFC_PARSE_STMT_IO_WRITE)
		|| !stmt->io.params)
		return NULL;

	ofc_sema_stmt_t s;
	s.type = OFC_SEMA_STMT_IO_WRITE;
	s.io_write.unit         = NULL;
	s.io_write.stdout       = false;
	s.io_write.format       = NULL;
	s.io_write.format_ldio  = false;
	s.io_write.formatted    = false;
	s.io_write.advance      = NULL;
	s.io_write.err          = NULL;
	s.io_write.iostat       = NULL;
	s.io_write.rec          = NULL;
	s.io_write.iolist       = NULL;

	ofc_parse_call_arg_t* ca_unit    = NULL;
	ofc_parse_call_arg_t* ca_format  = NULL;
	ofc_parse_call_arg_t* ca_iostat  = NULL;
	ofc_parse_call_arg_t* ca_rec     = NULL;
	ofc_parse_call_arg_t* ca_err     = NULL;
	ofc_parse_call_arg_t* ca_advance = NULL;

	unsigned i;
	for (i = 0; i < stmt->io.params->count; i++)
	{
		ofc_parse_call_arg_t* param
			= stmt->io.params->call_arg[i];
		if (!param) continue;

		if (ofc_sparse_ref_empty(param->name))
		{
			if (i >= 2)
			{
				ofc_sparse_ref_error(param->src,
					"Un-named parameter %u has no meaning in WRITE.", i);
				return NULL;
			}

			if (i == 0)
			{
				ca_unit = param;
			}
			else
			{
				if (!ca_unit)
				{
					ofc_sparse_ref_error(param->src,
						"Un-named format parameter only valid after UNIT in WRITE.");
					return NULL;
				}

				ca_format = param;
			}
		}
		else if (ofc_str_ref_equal_strz_ci(param->name.string, "UNIT"))
		{
			if (ca_unit)
			{
				ofc_sparse_ref_error(param->src,
					"Re-definition of UNIT in WRITE.");
				return NULL;
			}

			ca_unit = param;
		}
		else if (ofc_str_ref_equal_strz_ci(param->name.string, "FMT"))
		{
			if (ca_format)
			{
				ofc_sparse_ref_error(param->src,
					"Re-definition of FMT in WRITE.");
				return NULL;
			}

			ca_format = param;
		}
		else if (ofc_str_ref_equal_strz_ci(param->name.string, "IOSTAT"))
		{
			if (ca_iostat)
			{
				ofc_sparse_ref_error(param->src,
					"Re-definition of IOSTAT in WRITE.");
				return NULL;
			}

			ca_iostat = param;
		}
		else if (ofc_str_ref_equal_strz_ci(param->name.string, "REC"))
		{
			if (ca_rec)
			{
				ofc_sparse_ref_error(param->src,
					"Re-definition of REC in WRITE.");
				return NULL;
			}

			ca_rec = param;
		}
		else if (ofc_str_ref_equal_strz_ci(param->name.string, "ERR"))
		{
			if (ca_err)
			{
				ofc_sparse_ref_error(param->src,
					"Re-definition of ERR in WRITE.");
				return NULL;
			}

			ca_err = param;
		}
		else if (ofc_str_ref_equal_strz_ci(param->name.string, "ADVANCE"))
		{
			if (ca_advance)
			{
				ofc_sparse_ref_error(param->src,
					"Re-definition of ADVANCE in WRITE.");
				return NULL;
			}

			ca_advance = param;
		}
		else
		{
			ofc_sparse_ref_error(param->src,
				"Unrecognized paramater %u name '%.*s' in WRITE.",
				i, param->name.string.size, param->name.string.base);
			return NULL;
		}
	}

	if (!ca_unit)
	{
		ofc_sparse_ref_error(stmt->src,
			"No UNIT defined in WRITE.");
		return NULL;
	}

	if (ca_unit->type == OFC_PARSE_CALL_ARG_ASTERISK)
	{
		s.io_write.stdout = true;
	}
	else if (ca_unit->type == OFC_PARSE_CALL_ARG_EXPR)
	{
		s.io_write.unit = ofc_sema_expr(
			scope, ca_unit->expr);
		if (!s.io_write.unit) return NULL;

		const ofc_sema_type_t* etype
			= ofc_sema_expr_type(s.io_write.unit);
		if (!etype)
		{
			ofc_sema_stmt_io_write__cleanup(s);
			return NULL;
		}

		if (!ofc_sema_type_is_character(etype)
			&& (!ofc_sema_type_is_integer(etype)
				|| !ofc_sema_expr_validate_uint(s.io_write.unit)))
		{
			ofc_sparse_ref_error(stmt->src,
				   "UNIT must be a positive INTEGER "
				   "or a CHARACTER expression in WRITE");
			ofc_sema_stmt_io_write__cleanup(s);
			return NULL;
		}
	}
	else
	{
		ofc_sparse_ref_error(stmt->src,
			"UNIT must be an INTEGER or CHARACTER "
			"expression, or asterisk in WRITE");
		return NULL;
	}

	if (ca_format && (ca_format->type == OFC_PARSE_CALL_ARG_ASTERISK))
	{
		s.io_write.format_ldio = true;
		s.io_write.formatted   = true;
	}
	else if (ca_format && (ca_format->type == OFC_PARSE_CALL_ARG_EXPR))
	{
		s.io_write.formatted = true;

		s.io_write.format = ofc_sema_expr(
			scope, ca_format->expr);
		if (!s.io_write.format)
		{
			ofc_sema_stmt_io_write__cleanup(s);
			return NULL;
		}

		const ofc_sema_type_t* etype
			= ofc_sema_expr_type(s.io_write.format);
		if (!etype)
		{
			ofc_sema_stmt_io_write__cleanup(s);
			return NULL;
		}

		if (ofc_sema_type_is_integer(etype))
		{
			s.io_write.format->is_label  = true;
			s.io_write.format->is_format = true;
		}
		else if (etype->type != OFC_SEMA_TYPE_CHARACTER)
		{
			/* TODO - Support INTEGER array formats. */

			ofc_sparse_ref_error(stmt->src,
				"Format (FMT) must be a label or character string in WRITE");
			ofc_sema_stmt_io_write__cleanup(s);
			return NULL;
		}
	}
	else if (ca_format)
	{
		ofc_sparse_ref_error(stmt->src,
			"Format (FMT) must be an INTEGER expression or asterisk in WRITE");
		ofc_sema_stmt_io_write__cleanup(s);
		return NULL;
	}

	if (ca_advance && s.io_write.stdout)
	{
		ofc_sparse_ref_error(stmt->src,
			"ADVANCE specifier can only be used with an external UNIT in WRITE");
		ofc_sema_stmt_io_write__cleanup(s);
		return NULL;
	}
	else if (ca_advance && (!ca_format || s.io_write.format_ldio))
	{
		ofc_sparse_ref_error(stmt->src,
			"ADVANCE specifier can only be used with a formatted input in WRITE");
		ofc_sema_stmt_io_write__cleanup(s);
		return NULL;
	}
	else if (ca_advance)
	{
		s.io_write.advance = ofc_sema_expr(
			scope, ca_advance->expr);
		if (!s.io_write.advance)
		{
			ofc_sema_stmt_io_write__cleanup(s);
			return NULL;
		}

		const ofc_sema_type_t* etype
			= ofc_sema_expr_type(s.io_write.advance);
		if (!etype)
		{
			ofc_sema_stmt_io_write__cleanup(s);
			return NULL;
		}

		if (etype->type != OFC_SEMA_TYPE_CHARACTER)
		{
			ofc_sparse_ref_error(stmt->src,
				"ADVANCE must be a CHARACTER expression in WRITE");
			ofc_sema_stmt_io_write__cleanup(s);
			return NULL;
		}
		else
		{
			const ofc_sema_typeval_t* constant
				= ofc_sema_expr_constant(s.io_write.advance);

			if (constant
				&& !ofc_typeval_character_equal_strz_ci(constant, "NO")
				&& !ofc_typeval_character_equal_strz_ci(constant, "YES"))
			{
				ofc_sparse_ref_error(stmt->src,
					"ADVANCE must be YES/NO in WRITE");
				ofc_sema_stmt_io_write__cleanup(s);
				return NULL;
			}
		}
	}

	if (ca_iostat)
	{
		s.io_write.iostat = ofc_sema_expr(
			scope, ca_iostat->expr);
		if (!s.io_write.iostat)
		{
			ofc_sema_stmt_io_write__cleanup(s);
			return NULL;
		}

		if (s.io_write.iostat->type != OFC_SEMA_EXPR_LHS)
		{
			ofc_sparse_ref_error(stmt->src,
				"IOSTAT must be a variable in WRITE");
			ofc_sema_stmt_io_write__cleanup(s);
			return NULL;
		}

		const ofc_sema_type_t* etype
			= ofc_sema_expr_type(s.io_write.iostat);
		if (!etype)
		{
			ofc_sema_stmt_io_write__cleanup(s);
			return NULL;
		}

		if (!ofc_sema_type_is_integer(etype))
		{
			ofc_sparse_ref_error(stmt->src,
				"IOSTAT must be of type INTEGER in WRITE");
			ofc_sema_stmt_io_write__cleanup(s);
			return NULL;
		}
	}

	if (ca_rec && s.io_write.format_ldio)
	{
		ofc_sparse_ref_error(stmt->src,
			"REC specifier not compatible with namelist"
			" or list-directed data transfer in WRITE");
		ofc_sema_stmt_io_write__cleanup(s);
		return NULL;
	}
	else if (ca_rec)
	{
		s.io_write.rec = ofc_sema_expr(
			scope, ca_rec->expr);
		if (!s.io_write.rec)
		{
			ofc_sema_stmt_io_write__cleanup(s);
			return NULL;
		}

		const ofc_sema_type_t* etype
			= ofc_sema_expr_type(s.io_write.rec);
		if (!etype)
		{
			ofc_sema_stmt_io_write__cleanup(s);
			return NULL;
		}

		if (!ofc_sema_type_is_integer(etype))
		{
			ofc_sparse_ref_error(stmt->src,
				"REC must be of type INTEGER in WRITE");
			ofc_sema_stmt_io_write__cleanup(s);
			return NULL;
		}
	}

	if (ca_err)
	{
		s.io_write.err = ofc_sema_expr_label(
			scope, ca_err->expr);
		if (!s.io_write.err)
		{
			ofc_sema_stmt_io_write__cleanup(s);
			return NULL;
		}
	}

	/* Check iolist */
	if (stmt->io.iolist)
	{
		s.io_write.iolist
			= ofc_sema_expr_list_io(
				scope, stmt->io.iolist);
		if (!s.io_write.iolist)
		{
			ofc_sema_stmt_io_write__cleanup(s);
			return NULL;
		}
	}

	ofc_sema_stmt_t* as
		= ofc_sema_stmt_alloc(s);
	if (!as)
	{
		ofc_sema_stmt_io_write__cleanup(s);
		return NULL;
	}
	return as;
}

static bool ofc_sema_stmt_write__print_optional(
	ofc_colstr_t* cs, const char* name,
	const ofc_sema_expr_t* expr)
{
	if (!cs || !expr)
		return false;

	if (!ofc_colstr_atomic_writef(cs, ",")
		|| !ofc_colstr_atomic_writef(cs, " ")
		|| !ofc_colstr_atomic_writef(cs, name)
		|| !ofc_colstr_atomic_writef(cs, "=")
		|| !ofc_sema_expr_print(cs, expr))
		return false;

	return true;
}

bool ofc_sema_stmt_write_print(ofc_colstr_t* cs,
	const ofc_sema_stmt_t* stmt)
{
	if (!cs || (stmt->type != OFC_SEMA_STMT_IO_WRITE))
		return false;

	if (!ofc_colstr_atomic_writef(cs, "WRITE")
		|| !ofc_colstr_atomic_writef(cs, " ")
		|| !ofc_colstr_atomic_writef(cs, "("))
		return false;

	if (stmt->io_write.stdout)
	{
		if (!ofc_colstr_atomic_writef(cs, "*"))
			return false;
	}
	else
	{
		if (!ofc_sema_expr_print(cs,
			stmt->io_write.unit))
			return false;
	}

	if (stmt->io_write.format_ldio)
	{
		if (!ofc_colstr_atomic_writef(cs, ",")
			|| !ofc_colstr_atomic_writef(cs, " "))
			return false;
		if (!ofc_colstr_atomic_writef(cs, "*"))
			return false;
	}
	else if (stmt->io_write.formatted)
	{
		if (!ofc_sema_stmt_write__print_optional(
			cs, "FMT", stmt->io_write.format))
			return false;
	}

	if (stmt->io_write.iostat)
	{
		if (!ofc_sema_stmt_write__print_optional(
			cs, "IOSTAT", stmt->io_write.iostat))
			return false;
	}
	if (stmt->io_write.rec)
	{
		if (!ofc_sema_stmt_write__print_optional(
			cs,	"REC", stmt->io_write.rec))
			return false;
	}
	if (stmt->io_write.err)
	{
		if (!ofc_sema_stmt_write__print_optional(
			cs,	"ERR", stmt->io_write.err))
			return false;
	}
	if (stmt->io_write.advance)
	{
		if (!ofc_sema_stmt_write__print_optional(
			cs,	"ADVANCE", stmt->io_write.advance))
			return false;
	}

	if (!ofc_colstr_atomic_writef(cs, ")"))
		return false;

	if (stmt->io_write.iolist)
	{
		if (!ofc_colstr_atomic_writef(cs, " ")
			|| !ofc_sema_expr_list_print(cs,
				stmt->io_write.iolist))
			return false;
	}

	/* TODO - Handle advance */

	return true;
}
