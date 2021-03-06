# encoding: utf-8

#  Copyright (c) 2001-2014, Canal TP and/or its affiliates. All rights reserved.
#
# This file is part of Navitia,
#     the software to build cool stuff with public transport.
#
# Hope you'll enjoy and contribute to this project,
#     powered by Canal TP (www.canaltp.fr).
# Help us simplify mobility and open public transport:
#     a non ending quest to the responsive locomotion way of traveling!
#
# LICENCE: This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#
# Stay tuned using
# twitter @navitia
# IRC #navitia on freenode
# https://groups.google.com/d/forum/navitia
# www.navitia.io
import logging

from flask_restful import reqparse, abort
import flask_restful
from flask import current_app, request, g
from functools import wraps
from jormungandr.exceptions import RegionNotFound
import datetime
import base64
from navitiacommon.models import User, Instance, db


def authentification_required(func):
    """
    decorateur chargé de l'authentification des requetes
    fonctionne pour chaque API prenant un paramétre la région
    si la region est absente de la requéte la requete est automatique autorisé
    """
    @wraps(func)
    def wrapper(*args, **kwargs):
        region = None
        if 'region' in kwargs:
            region = kwargs['region']
            #TODO revoir comment on gere le lon/lat
        elif 'lon' in kwargs and 'lat' in kwargs:
            try:
                from jormungandr import i_manager  # quick fix to avoid circular dependencies
                region = i_manager.key_of_coord(lon=kwargs['lon'],
                                                lat=kwargs['lat'])
            except RegionNotFound:
                pass

        if not region:
            #we could not find any regions, we abort
            abort_request()

        if not region or authenticate(region, 'ALL', abort=True):
            return func(*args, **kwargs)

    return wrapper


def get_token():
    """
    find the Token in the "Authorization" HTTP header
    two cases are handle:
        - the token is the only value in the header
        - Basic Authentication is used and the token is in the username part
          In this case the Value of the header look like this:
          "BASIC 54651a4ae4rae"
          The second part is the username and the password separate by a ":"
          and encoded in base64
    """
    if 'Authorization' not in request.headers:
        return None

    args = request.headers['Authorization'].split(' ')
    if len(args) == 2:
        try:
            b64 = args[1]
            decoded = base64.decodestring(b64)
            return decoded.split(':')[0]
        except ValueError:
            return None
    else:
        return request.headers['Authorization']


def authenticate(region, api, abort=False):
    """
    Check the Authorization of the current user for this region and this API.
    If abort is True, the request is aborted with the appropriate HTTP code.
    """
    if 'PUBLIC' in current_app.config \
            and current_app.config['PUBLIC']:
        #if jormungandr is on public mode we skip the authentification process
        return True

    token = get_token()

    if not token:
        if abort:
            abort_request()
        else:
            return False

    user = get_user()
    if user:
        if user.has_access(region, api):
            return True
        else:
            if abort:
                abort_request()
            else:
                return False
    else:
        if abort:
           abort_request()
        else:
            return False

def abort_request():
    """
    abort a request with the proper http status in case of authentification issues
    """
    if get_user():
        flask_restful.abort(403)
    else:
        flask_restful.abort(401)

def has_access(instance, abort=False):
    if 'PUBLIC' in current_app.config \
            and current_app.config['PUBLIC']:
        #if jormungandr is on public mode we skip the authentification process
        return True
    res = instance.is_accessible_by(get_user())
    if abort and not res:
        abort_request()
    else:
        return res

def get_user():
    """
    return the current authenticated User or None
    """
    if hasattr(g, 'user'):
        return g.user
    else:
        token = get_token()
        if not token:
            flask_restful.abort(401)
        g.user = User.get_from_token(token, datetime.datetime.now())
        logging.info('user %s', g.user)

        return g.user
