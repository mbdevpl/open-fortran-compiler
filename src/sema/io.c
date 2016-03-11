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

#include "ofc/sema.h"

/* Compare type to descriptor type at
 * offset position in format_list
 */
bool ofc_sema_io_compare_types(
	const ofc_sema_stmt_t* stmt,
	const ofc_sema_lhs_t* lhs,
	ofc_sema_expr_t** expr,
	const ofc_sema_type_t* type,
	ofc_parse_format_desc_list_t* format_list,
	unsigned* offset)
{
	if (!type || !format_list || (!lhs && !expr))
		return false;

	if (ofc_sema_type_is_complex(type))
	{
		unsigned j;
		for (j = 0; j < 2; j++)
		{
			ofc_parse_format_desc_t* desc
				= format_list->desc[(*offset)++];
			if (!desc)
				return false;

			if (!ofc_sema_compare_desc_expr_type(desc->type, type->type))
			{
				ofc_sparse_ref_warning((!expr ? lhs->src : (*expr)->src),
					"Trying to format a %s output  with a %s FORMAT descriptor",
					ofc_sema_format_str_rep(desc->type),
					ofc_sema_type_str_rep(type));
			}
		}
		return true;
	}

	ofc_parse_format_desc_t* desc
		= format_list->desc[(*offset)++];
	if (!desc) return false;

	if (!ofc_sema_compare_desc_expr_type(desc->type, type->type))
	{
		if (!expr)
		{
			ofc_sparse_ref_warning(lhs->src,
				"Trying to format a %s output  with a %s FORMAT descriptor",
				ofc_sema_format_str_rep(desc->type),
				ofc_sema_type_str_rep(type));
		}
		else
		{
			const ofc_sema_type_t* dtype
				= ofc_sema_format_desc_type(desc);

			ofc_sema_expr_t* cast
				= ofc_sema_expr_cast(*expr, dtype);
			if (!cast)
			{
				ofc_sparse_ref_warning((*expr)->src,
					"Trying to format a %s output  with a %s FORMAT descriptor",
					ofc_sema_format_str_rep(desc->type),
					ofc_sema_type_str_rep(type));
			}
			else
			{
				*expr = cast;
			}
		}
	}
	else if (desc->type == OFC_PARSE_FORMAT_DESC_CHARACTER)
	{
		unsigned csize;
		if (!ofc_sema_type_base_size(type, &csize)
			|| (csize != 1))
		{
			ofc_sparse_ref_t src = (lhs ? lhs->src : (*expr)->src);
			ofc_sparse_ref_error(src,
				"CHARACTER type KIND not supported in %s",
				(stmt->type == OFC_SEMA_STMT_IO_WRITE ? "WRITE" : "PRINT"));
		}
	}

	return true;
}


bool ofc_sema_io_list_has_complex(
	ofc_sema_lhs_list_t* ilist,
	ofc_sema_expr_list_t* olist,
	unsigned* count)
{
	if (!count) return false;

	if (ilist)
	{
		unsigned i;
		for (i = 0; i < ilist->count; i++)
		{
			const ofc_sema_type_t* type
				= ofc_sema_lhs_type(ilist->lhs[i]);
			if (!type) return false;

			if (ofc_sema_type_is_complex(type))
			{
				unsigned elem_count;
				if (!ofc_sema_lhs_elem_count(
					ilist->lhs[i], &elem_count))
					return false;

				*count += elem_count;
			}
		}
	}
	else if (olist)
	{
		unsigned i;
		for (i = 0; i < olist->count; i++)
		{
			const ofc_sema_type_t* type
				= ofc_sema_expr_type(olist->expr[i]);
			if (!type) return false;

			if (ofc_sema_type_is_complex(type))
			{
				unsigned elem_count;
				if (!ofc_sema_expr_elem_count(
					olist->expr[i], &elem_count))
					return false;

				*count += elem_count;
			}
		}
	}
	else
	{
		return false;
	}

	return true;
}

static bool ofc_sema_io__data_format_helper_r(
	ofc_parse_format_desc_list_t* format_list,
	const ofc_parse_format_desc_list_t* format_src,
	unsigned* iolist_len);

static bool ofc_sema_io__data_format_helper_body(
	ofc_parse_format_desc_list_t* format_list,
	ofc_parse_format_desc_t* desc,
	unsigned* iolist_len)
{
	if (ofc_parse_format_is_data_desc(desc))
	{
		/* If it's a data descriptor, we add it to the list
		   and call the function again to the next offset
		   We are an element closer to have length iolist_len. */

		unsigned i;
		for (i = 0; (i < desc->n) && (*iolist_len > 0); i++)
		{
			ofc_parse_format_desc_t* cdesc
				= ofc_parse_format_desc_copy(desc);
			if (!cdesc) return false;

			if (!ofc_parse_format_desc_list_add(format_list, cdesc))
			{
				ofc_parse_format_desc_delete(cdesc);
				return false;
			}

			(*iolist_len)--;
		}
	}
	else if (desc->type == OFC_PARSE_FORMAT_DESC_REPEAT)
	{
		/* If the descriptor is of type repeat, we call
		   the function again for the sub-format-list. */

		unsigned i;
		for (i = 0; (i < desc->n)  && (*iolist_len > 0); i++)
		{
			if (!ofc_sema_io__data_format_helper_r(
				format_list, desc->repeat, iolist_len))
				return false;
		}
	}
	else
	{
		/* If it's a different type, we ignore it
		   and continue with the next offset. */
	}

	return true;
}

static bool ofc_sema_io__data_format_helper_r(
	ofc_parse_format_desc_list_t* format_list,
	const ofc_parse_format_desc_list_t* format_src,
	unsigned* iolist_len)
{
	unsigned offset;
	for (offset = 0; (*iolist_len > 0)
		&& (offset < format_src->count); offset++)
	{
		if (!ofc_sema_io__data_format_helper_body(
			format_list, format_src->desc[offset], iolist_len))
			return false;
	}

	return true;
}

static bool ofc_sema_io__data_format_helper(
	ofc_parse_format_desc_list_t* format_list,
	const ofc_parse_format_desc_list_t* format_src,
	unsigned iolist_len)
{
	if (!format_list || !format_src)
		return false;

	/* This is to handle "forced reversion"
	   http://www.obliquity.com/computer/fortran/format.html */
	unsigned repeat_from = 0;
	unsigned offset;
	for (offset = 0; offset < format_src->count; offset++)
	{
		if (format_src->desc[offset]->type
			== OFC_PARSE_FORMAT_DESC_REPEAT)
			repeat_from = offset;
	}

	unsigned remain;
	for (offset = 0, remain = iolist_len; remain > 0; offset++)
	{
		if (offset >= format_src->count)
		{
			/* If we didn't see a data descriptor
			   then stop iterating. */
			if (remain == iolist_len)
				break;

			offset = repeat_from;

			/* Reset the original length to the current length
			   so we can detect if there's no data descriptor
			   after the repeat_from offset. */
			iolist_len = remain;
		}

		if (!ofc_sema_io__data_format_helper_body(
			format_list, format_src->desc[offset], &remain))
			return false;
	}

	return true;
}

ofc_parse_format_desc_list_t* ofc_sema_io_data_format(
	const ofc_parse_format_desc_list_t* format, unsigned iolist_len)
{
	if (!format) return NULL;

	ofc_parse_format_desc_list_t* format_list
		= ofc_parse_format_desc_list_create();

	if (!ofc_sema_io__data_format_helper(
		format_list, format, iolist_len))
	{
		ofc_parse_format_desc_list_delete(format_list);
		return NULL;
	}

	return format_list;
}

unsigned ofc_sema_io_data_format_count(
	const ofc_parse_format_desc_list_t* format)
{
	if (!format) return 0;

	unsigned i, count = 0;
	for (i = 0; i < format->count; i++)
	{
		ofc_parse_format_desc_t* desc
			= format->desc[i];

		if (ofc_parse_format_is_data_desc(desc))
		{
			count += desc->n;
		}
		else if (desc->type == OFC_PARSE_FORMAT_DESC_REPEAT)
		{
			unsigned n;
			for (n = 0; n < desc->n; n++)
			{
				count += ofc_sema_io_data_format_count(desc->repeat);
			}
		}
	}

	return count;
}

static ofc_parse_format_desc_t* ofc_sema_io_format_iolist_check_def__helper(
	ofc_parse_format_desc_t* desc,
	ofc_sema_expr_list_t* iolist,
	unsigned* offset, bool* changed)
{
	if (!desc || !iolist)
		return NULL;

	if (desc->type == OFC_PARSE_FORMAT_DESC_REPEAT)
	{
		ofc_parse_format_desc_list_t* repeat_list
			= ofc_parse_format_desc_list_create();
		if (!repeat_list) return NULL;

		bool was_changed = false;
		unsigned k;
		for (k = 0; k < desc->repeat->count; k++)
		{
			bool c = false;
			ofc_parse_format_desc_t* elem
				= ofc_sema_io_format_iolist_check_def__helper(
					desc->repeat->desc[k], iolist, offset, &c);
			if (c) was_changed = true;
			if (!ofc_parse_format_desc_list_add(repeat_list, elem))
			{
				ofc_parse_format_desc_list_delete(repeat_list);
				return NULL;
			}
		}

		if (changed) *changed = was_changed;
		ofc_parse_format_desc_t* repeat
			= ofc_parse_format_desc_create_repeat(repeat_list, desc->n);
		return repeat;
	}
	else if (!ofc_parse_format_is_data_desc(desc))
	{
		if (changed) *changed = false;
		return ofc_parse_format_desc_copy(desc);
	}
	else
	{
		unsigned repeat;
		if (!ofc_parse_format_desc_elem_count(
			desc, &repeat))
			return NULL;

		ofc_sema_expr_t* expr
			= ofc_sema_expr_list_elem_get(iolist, *offset);
		*offset += repeat;

		if (!expr)
			return ofc_parse_format_desc_copy(desc);

		ofc_parse_format_desc_t* copy
			= ofc_sema_format_desc_set_def(desc, expr, NULL);
		if (!copy)
		{
			ofc_sema_expr_delete(expr);
			return NULL;
		}

		bool is_same = ofc_parse_format_desc_compare(desc, copy);
		if (changed) *changed = !is_same;


		ofc_sema_expr_delete(expr);

		return copy;
	}

	return NULL;
}

static ofc_parse_format_desc_t* ofc_sema_io_format_input_list_check_def__helper(
	ofc_parse_format_desc_t* desc,
	ofc_sema_lhs_list_t* iolist,
	unsigned* offset, bool* changed)
{
	if (!desc || !iolist)
		return NULL;

	if (desc->type == OFC_PARSE_FORMAT_DESC_REPEAT)
	{
		ofc_parse_format_desc_list_t* repeat_list
			= ofc_parse_format_desc_list_create();
		if (!repeat_list) return NULL;

		bool was_changed = false;
		unsigned k;
		for (k = 0; k < desc->repeat->count; k++)
		{
			bool c = false;
			ofc_parse_format_desc_t* elem
				= ofc_sema_io_format_input_list_check_def__helper(
					desc->repeat->desc[k], iolist, offset, &c);
			if (c) was_changed = true;
			if (!ofc_parse_format_desc_list_add(repeat_list, elem))
			{
				ofc_parse_format_desc_list_delete(repeat_list);
				return NULL;
			}
		}

		if (changed) *changed = was_changed;
		ofc_parse_format_desc_t* repeat
			= ofc_parse_format_desc_create_repeat(repeat_list, desc->n);
		return repeat;
	}
	else if (!ofc_parse_format_is_data_desc(desc))
	{
		if (changed) *changed = false;
		return ofc_parse_format_desc_copy(desc);
	}
	else
	{
		unsigned repeat;
		if (!ofc_parse_format_desc_elem_count(
			desc, &repeat))
			return NULL;

		ofc_sema_lhs_t* lhs
			= ofc_sema_lhs_list_elem_get(iolist, *offset);
		*offset += repeat;

		if (!lhs)
			return ofc_parse_format_desc_copy(desc);

		ofc_parse_format_desc_t* copy
			= ofc_sema_format_desc_set_def(desc, NULL, lhs);
		if (!copy)
		{
			ofc_sema_lhs_delete(lhs);
			return NULL;
		}

		bool is_same = ofc_parse_format_desc_compare(desc, copy);
		if (changed) *changed = is_same;

		ofc_sema_lhs_delete(lhs);

		return copy;
	}

	return NULL;
}

bool ofc_sema_io_format_iolist_check_def(
	ofc_sema_stmt_t* stmt,
	ofc_sema_expr_list_t* iolist)
{
	if (!stmt || stmt->type != OFC_SEMA_STMT_IO_FORMAT || !iolist)
		return false;

	ofc_parse_format_desc_list_t* format_list_default
		= ofc_parse_format_desc_list_create();
	if (!format_list_default) return false;

	const ofc_parse_format_desc_list_t* format_list
		= stmt->io_format.src;

	bool different = false;
	bool changed = false;
	unsigned i, j;
	for (i = 0, j = 0; i < format_list->count; i++)
	{
		ofc_parse_format_desc_t* desc
			= format_list->desc[i];
		if (!desc)
		{
			ofc_parse_format_desc_list_delete(format_list_default);
			return false;
		}

		ofc_parse_format_desc_t* copy
			= ofc_sema_io_format_iolist_check_def__helper(
				desc, iolist, &j, &different);
		if (!copy)
		{
			ofc_parse_format_desc_list_delete(format_list_default);
			return false;
		}

		if (different) changed = true;

		if (!ofc_parse_format_desc_list_add(
				format_list_default, copy))
		{
			ofc_parse_format_desc_delete(copy);
			ofc_parse_format_desc_list_delete(format_list_default);
			return false;
		}
	}

	if (changed)
	{
		/* If there was already a format->format, check if they are compatible
		   if they are different, delete format->format, and set
		   format->is_default_possible to false*/
		if (stmt->io_format.format)
		{
			unsigned i;
			for (i = 0; i < format_list_default->count; i++)
			{
				ofc_parse_format_desc_t* desc_orig
					= stmt->io_format.format->desc[i];

				ofc_parse_format_desc_t* desc_new
					= format_list_default->desc[i];

				stmt->io_format.is_default_possible
					= ofc_parse_format_desc_compare(desc_orig, desc_new);

				if (!stmt->io_format.is_default_possible) break;
			}

			if (!stmt->io_format.is_default_possible)
			{
				ofc_parse_format_desc_list_delete(stmt->io_format.format);
				stmt->io_format.format = NULL;
			}

			ofc_parse_format_desc_list_delete(format_list_default);
		}
		else if (stmt->io_format.is_default_possible)
		{
			stmt->io_format.format = format_list_default;
		}
		else
		{
			ofc_parse_format_desc_list_delete(format_list_default);
			return false;
		}
	}
	else
	{
		ofc_parse_format_desc_list_delete(format_list_default);
	}

	return true;
}

bool ofc_sema_io_format_input_list_check_def(
	ofc_sema_stmt_t* stmt,
	ofc_sema_lhs_list_t* ilist)
{
	if (!stmt || stmt->type != OFC_SEMA_STMT_IO_FORMAT || !ilist)
		return false;

	ofc_parse_format_desc_list_t* format_list_default
		= ofc_parse_format_desc_list_create();
	if (!format_list_default) return false;

	const ofc_parse_format_desc_list_t* format_list
		= stmt->io_format.src;

	bool different = false;
	bool changed = false;
	unsigned i, j;
	for (i = 0, j = 0; i < format_list->count; i++)
	{
		ofc_parse_format_desc_t* desc
			= format_list->desc[i];
		if (!desc)
		{
			ofc_parse_format_desc_list_delete(format_list_default);
			return false;
		}

		ofc_parse_format_desc_t* copy
			= ofc_sema_io_format_input_list_check_def__helper(
				desc, ilist, &j, &different);
		if (!copy)
		{
			ofc_parse_format_desc_list_delete(format_list_default);
			return false;
		}

		if (different) changed = true;

		if (!ofc_parse_format_desc_list_add(
				format_list_default, copy))
		{
			ofc_parse_format_desc_delete(copy);
			ofc_parse_format_desc_list_delete(format_list_default);
			return false;
		}
	}

	if (changed)
	{
		/* If there was already a format->format, check if they are compatible
		   if they are different, delete format->format, and set
		   format->is_default_possible to false*/
		if (stmt->io_format.format)
		{
			unsigned i;
			for (i = 0; i < format_list_default->count; i++)
			{
				ofc_parse_format_desc_t* desc_orig
					= stmt->io_format.format->desc[i];

				ofc_parse_format_desc_t* desc_new
					= format_list_default->desc[i];

				stmt->io_format.is_default_possible
					= ofc_parse_format_desc_compare(desc_orig, desc_new);

				if (!stmt->io_format.is_default_possible) break;
			}

			if (!stmt->io_format.is_default_possible)
			{
				ofc_parse_format_desc_list_delete(stmt->io_format.format);
				stmt->io_format.format = NULL;
			}

			ofc_parse_format_desc_list_delete(format_list_default);
		}
		else if (stmt->io_format.is_default_possible)
		{
			stmt->io_format.format = format_list_default;
		}
		else
		{
			ofc_parse_format_desc_list_delete(format_list_default);
			return false;
		}
	}
	else
	{
		ofc_parse_format_desc_list_delete(format_list_default);
	}

	return true;
}

bool ofc_sema_io_format_iolist_compare(
	const ofc_sema_stmt_t* stmt,
	ofc_parse_format_desc_list_t* format_list,
	ofc_sema_expr_list_t* iolist)
{
	if (!format_list || !iolist) return false;

	unsigned i, offset = 0;
	for (i = 0; i < iolist->count; i++)
	{
		ofc_sema_expr_t* pexpr
			= ofc_sema_expr_list_elem_get(iolist, i);

		const ofc_sema_type_t* type
			= ofc_sema_expr_type(pexpr);

		if (!ofc_sema_io_compare_types(
			stmt, NULL, &pexpr, type, format_list, &offset))
			return false;

		ofc_sema_expr_delete(pexpr);
	}

	return true;
}

bool ofc_sema_io_format_input_list_compare(
	const ofc_sema_stmt_t* stmt,
	ofc_parse_format_desc_list_t* format_list,
	ofc_sema_lhs_list_t* iolist)
{
	if (!format_list || !iolist) return false;

	unsigned i, offset = 0;
	for (i = 0; i < iolist->count; i++)
	{
		ofc_sema_lhs_t* lhs
			= ofc_sema_lhs_list_elem_get(iolist, i);
		const ofc_sema_type_t* type
			= ofc_sema_lhs_type(lhs);

		if (!ofc_sema_io_compare_types(
			stmt, lhs, NULL, type, format_list, &offset))
			return false;

		ofc_sema_lhs_delete(lhs);
	}

	return true;
}



bool ofc_sema_stmt_io_format_validate(
	ofc_sema_stmt_t* stmt)
{
	if (!stmt) return false;

	ofc_sema_expr_t* format_expr = NULL;
	ofc_sema_lhs_list_t*  ilist = NULL;
	ofc_sema_expr_list_t* olist = NULL;
	switch (stmt->type)
	{
		case OFC_SEMA_STMT_IO_WRITE:
			olist = stmt->io_write.iolist;
			format_expr = stmt->io_write.format;
			break;

		case OFC_SEMA_STMT_IO_READ:
			ilist = stmt->io_read.iolist;
			format_expr = stmt->io_read.format;
			break;

		case OFC_SEMA_STMT_IO_PRINT:
			olist = stmt->io_print.iolist;
			format_expr = stmt->io_print.format;
			break;

		default:
			return false;
	}

	if ((!ilist && !olist)
		|| !format_expr)
		return true;

	ofc_sema_label_t* label
		= format_expr->label;
	if (!format_expr->is_label)
	{
		/* TODO - Check CHARACTER FORMAT descriptors. */
		return true;
	}
	else if (!label)
	{
		/* Label isn't resolvable at compile-time */
		return true;
	}

	if ((label->type != OFC_SEMA_LABEL_STMT)
		|| !label->stmt
		|| (label->stmt->type != OFC_SEMA_STMT_IO_FORMAT))
	{
		ofc_sparse_ref_error(stmt->src,
			"FORMAT label must point to a FORMAT statement");
		return false;
	}

	const ofc_parse_format_desc_list_t* format
		= label->stmt->io_format.format;
	if (!format) format = label->stmt->io_format.src;
	if (!format) return false;

	unsigned data_desc_count
		= ofc_sema_io_data_format_count(format);

	unsigned iolist_len = 0;
	if (ilist ? !ofc_sema_lhs_list_elem_count(ilist, &iolist_len)
		: !ofc_sema_expr_list_elem_count(olist, &iolist_len))
	{
		/* iolist has variable length; we can't check it at compile-time. */
		return true;
	}

	if (iolist_len == 0)
	{
		if (data_desc_count > 0)
		{
			ofc_sparse_ref_warning(format_expr->src,
				"No IO list in formatted IO statement");
		}

		/* iolist is empty; nothing to validate. */
		return true;
	}

	/* Complex types count as two elements for an iolist. */
	unsigned count = 0;
	if (!ofc_sema_io_list_has_complex(
		ilist, olist, &count))
		return false;
	iolist_len += count;

	if (data_desc_count == 0)
	{
		if (iolist_len > 0)
		{
			ofc_sparse_ref_warning(format_expr->src,
				"No data edit descriptors in FORMAT list");
		}

		/* No data descriptors to check. */
		return true;
	}

	if (iolist_len < data_desc_count)
	{
		ofc_sparse_ref_warning(format_expr->src,
			"IO list shorter than FORMAT list,"
			" last FORMAT data descriptors will be ignored");
	}
	else if ((iolist_len % data_desc_count) != 0)
	{
		ofc_sparse_ref_warning(format_expr->src,
			"IO list length is not a multiple of FORMAT list length");
	}

	/* Create a format list of same length as iolist
	 * with only data edit descriptors  */
	ofc_parse_format_desc_list_t* format_list
		= ofc_sema_io_data_format(format, iolist_len);
	if (!format_list) return false;

	/* Compare iolist with format */
	bool success;
	if (ilist)
	{
		success = ofc_sema_io_format_input_list_compare(
			stmt, format_list, ilist);
	}
	else
	{
		success = ofc_sema_io_format_iolist_compare(
			stmt, format_list, olist);
	}

	ofc_parse_format_desc_list_delete(format_list);
	return success;
}

bool ofc_sema_stmt_io_format_validate_defaults(
	ofc_sema_stmt_t* stmt, ofc_sema_stmt_t* format_stmt)
{
	if (!stmt) return false;

	ofc_sema_expr_t* format_expr = NULL;
	ofc_sema_lhs_list_t*  ilist = NULL;
	ofc_sema_expr_list_t* olist = NULL;
	switch (stmt->type)
	{
		case OFC_SEMA_STMT_IO_WRITE:
			olist = stmt->io_write.iolist;
			format_expr = stmt->io_write.format;
			break;

		case OFC_SEMA_STMT_IO_READ:
			ilist = stmt->io_read.iolist;
			format_expr = stmt->io_read.format;
			break;

		case OFC_SEMA_STMT_IO_PRINT:
			olist = stmt->io_print.iolist;
			format_expr = stmt->io_print.format;
			break;

		default:
			return false;
	}

	if ((!ilist && !olist)
		|| !format_expr)
		return true;

	ofc_sema_label_t* label
		= format_expr->label;
	if (!format_expr->is_label)
	{
		/* CHARACTER FORMAT descriptors can't be modified */
		return true;
	}
	else if (!label)
	{
		/* Label isn't resolvable at compile-time */
		return true;
	}

	if ((label->type != OFC_SEMA_LABEL_STMT)
		|| !label->stmt
		|| (label->stmt->type != OFC_SEMA_STMT_IO_FORMAT)
		|| (label->stmt != format_stmt))
		return true;

	if (label->stmt->io_format.is_default_possible)
	{
		if (ilist)
		{
			ofc_sema_io_format_input_list_check_def(
				format_stmt, ilist);
		}
		else
		{
			ofc_sema_io_format_iolist_check_def(
				format_stmt, olist);
		}
	}
	else
	{
		return true;
	}

	const ofc_parse_format_desc_list_t* format
		= format_stmt->io_format.format;
	if (!format) return true;

	unsigned data_desc_count
		= ofc_sema_io_data_format_count(format);

	unsigned iolist_len = 0;
	if (ilist ? !ofc_sema_lhs_list_elem_count(ilist, &iolist_len)
		: !ofc_sema_expr_list_elem_count(olist, &iolist_len))
	{
		/* iolist has variable length; we can't check it at compile-time. */
		return true;
	}

	if (iolist_len == 0)
	{
		/* iolist is empty; nothing to validate. */
		return true;
	}

	/* Complex types count as two elements for an iolist. */
	unsigned count = 0;
	if (!ofc_sema_io_list_has_complex(
		ilist, olist, &count))
		return false;
	iolist_len += count;

	if (data_desc_count == 0)
	{
		/* No data descriptors to check. */
		return true;
	}

	/* Create a format list of same length as iolist
	 * with only data edit descriptors  */
	ofc_parse_format_desc_list_t* format_list
		= ofc_sema_io_data_format(format, iolist_len);
	if (!format_list) return false;

	/* Compare iolist with format to validate the defaults */
	bool success;
	if (ilist)
	{
		success = ofc_sema_io_format_input_list_compare(
			stmt, format_list, ilist);
	}
	else
	{
		success = ofc_sema_io_format_iolist_compare(
			stmt, format_list, olist);
	}

	if (!success)
	{
		ofc_parse_format_desc_list_delete(format_stmt->io_format.format);
		format_stmt->io_format.format = NULL;
		format_stmt->io_format.is_default_possible = false;
	}
	ofc_parse_format_desc_list_delete(format_list);
	return true;
}
